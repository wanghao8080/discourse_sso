/**
 * @file
 * Monkey patch the Discourse Ember app.
 *
 * Provide client side modifications to allow the Discourse Ember application
 * to work via the server side Drupal discourse module as an intermediary to the main
 * Discourse forum server.
 */


// Namespace our functions.
Drupal.discourse = {};


// Tell Discourse to stop initialising until we are ready.
Discourse.deferReadiness();


/*
 * Rewrite html string before render.
 * Adds a prefix to link href.
 * Adds original domain name to image src.
 *
 * @param html String
 * @return String
 */
Drupal.discourse.rewriteHtml = function(html) {
    // Handle SafeString.{string: '...', toString()} objects.
    if (html instanceof Handlebars.SafeString) {
        html.string = Drupal.discourse.rewriteHtml(html.string);
        return html;
    }
    // Rewrite style tags
    html = html.replace('url(/uploads', 'url(' + Drupal.settings.discourse.server + '/uploads');
    // Rewrite href/src/etc which start with "/" but do not start with "/DRUPAL_FORUM_ROOT"
    // Replace ="/NOT_FORUM_ROOT
    var re1 = new RegExp('="/((?!' + Drupal.settings.discourse.rootURL.substring(1) + '))', 'gm');
    html = html.replace(re1, '="' + Drupal.settings.discourse.rootURL +'/');
    // Replace ='/NOT_FORUM_ROOT
    var re2 = new RegExp('=\'/((?!' + Drupal.settings.discourse.rootURL.substring(1) + '))', 'gm');
    html = html.replace(re2, '=\'' + Drupal.settings.discourse.rootURL +'/');
    return html;
};


/**
 * Rewrite the full path to remove the Drupal subpath and return the Ember path.
 * @param path String
 * @return String
 */
Drupal.discourse.rewritePath = function(path) {
    path = path.replace(/https?\:\/\/[^\/]+/, '');
    // TODO fix this regex so it only replaces at the start of the path.
    var rootURLregex = Drupal.settings.discourse.rootURL.replace(/\/$/, '');
    return path.replace(rootURLregex, '');
};


$(document).ready(function() {
    // Alter any Discourse ajax calls that they have the csrf token added to the header.
    var csrfToken = $('meta[name=csrf-token]').attr('content');
    var isDrupalAjaxCall = function(options) {
        // See if the beforeSend callback contains Drupal code. @see Drupal.ajax
        return (options.beforeSend && options.beforeSend.toString().indexOf('ajax.ajaxing') !== -1);
    };
    $.ajaxPrefilter(function(options, originalOptions, xhr) {
        if (!isDrupalAjaxCall(options)) {
            // @todo remove this if Discourse ever supports subdirectories natively.
            if (options.url.indexOf(Drupal.settings.discourse.rootURL) == -1) {
                options.url = Drupal.settings.discourse.rootURL + options.url;
            }
            // Put the csrf token in. This is cross domain so Discourse own code will not add it.
            xhr.setRequestHeader('X-CSRF-Token', csrfToken);
        }
    });
    // Intercept $LAB
    var $LAB_script = $LAB.script;
    $LAB.script = function (path){
        return $LAB_script(Drupal.settings.discourse.server + path)
    };
    // Rewrite links in translations
    I18n._translate = I18n.translate;
    I18n.translate = function(scope, options) {
        return Drupal.discourse.rewriteHtml(I18n._translate(scope, options));
    };
    I18n.t = I18n.translate;

    // Patch Morph.parseHTML in order to rewrite image links.
    Ember.View.renderer._dom._parseHTML = Ember.View.renderer._dom.parseHTML;
    Ember.View.renderer._dom.parseHTML = function(html, contextualElement) {
        html = Drupal.discourse.rewriteHtml(html);
        return Ember.View.renderer._dom._parseHTML(html, contextualElement);
    };

    // Allow Discourse to continue initialising now that we've made our modifications.
    Discourse.advanceReadiness();
});


/*
 * Patch Discourse.ready so we can change the rootURL for the Discourse app.
 */
Drupal.discourse._ready = Discourse.ready;
Discourse.ready = function() {

    // Set the root URL
    Discourse.BaseUri = Drupal.settings.discourse.rootURL;

    // Patch the Ember renderer so we can rewrite links and image references.
    Ember.View.renderer.buffer.innerContent = function () {
        if (this.buffer && typeof this.buffer === 'string') {
            this.buffer = Drupal.discourse.rewriteHtml(this.buffer);
        }
        return this.buffer;
    };

    // Patch Discourse to remove Drupal base path and forum prefix.
    Discourse.URL._replaceState = Discourse.URL.replaceState;
    Discourse.URL.replaceState = function (path) {
        path = Drupal.discourse.rewritePath(path);
        Discourse.URL._replaceState(path);
    };

    // Patch Discourse to get static pages working. /faq /guidelines etc.
    Discourse._getURL = Discourse.getURL;
    Discourse.getURL = function(url) {
        // Static urls are coming in as faq.html rather than /faq.html Fix this here.
        if (url.substring(url.length - 5) == '.html'  && url[url.length-1] !== '/') {
            url = '/' + url;
        }
        return Discourse._getURL(url);
    };

    Drupal.discourse.discourse_location = Ember.get(Discourse.__container__.lookup('router:main'), 'location');
    Drupal.discourse.discourse_location.rootURL = Drupal.settings.discourse.rootURL;

    // Replace Discourse logout function with Drupal logout.
    Discourse.User.logout = function () {
        window.location.href = '/user/logout';
    };

    Drupal.discourse._ready();
};
