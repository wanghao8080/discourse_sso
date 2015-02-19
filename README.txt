# Drupal - Discourse integration

This module provides a way to embed a Discourse forum into a Drupal website.

## Installation

* Download and enable this module, and the querypath module.
* Set the discourse server to use at /admin/config/system/discourse
* Log into the Discourse instance as an admin and go to the site settings screen /admin/site_settings
  Set force_hostname to be the Drupal url and path in Discourse site settings.
  Eg drupalsite.com/discourse This will give correct links in emails sent by Discourse.
* Your forum should now be available at /discourse

## Discourse version

Currently tested with
* Discourse Version: 1.2.0.beta5


## Single Sign On

To log in Drupal users to Discourse automatically you will need to set up Discourse to use SSO with the Drupal site providing authentication. The module provides an sso url for Discourse at /discourse_sso

enable_sso : must be enabled, global switch
sso_url: the offsite URL users will be sent to when attempting to log on. (This will be http://yourdrupalsite.com/discourse_sso).
sso_secret: a secret string used to hash SSO payloads. Ensures payloads are authentic.

https://meta.discourse.org/t/official-single-sign-on-for-discourse/13045


## Known limitations

* You cannot use a root forum path that matches a root path in Discourse
  e.g. /t /category etc.
* The Discourse pages load themselves with javascript rather than page reloads,
  so any Drupal content (blocks etc.) will need to be consistent across the
  Discourse forum.
* There are multiple issues with using drush rs to run the Drupal site during development.
* If using 'drush rs' to run the server, you will need to ensure you use the cgi
  version. 'drush rs server=cgi' Otherwise paths like
  'http://127.0.0.1:8888/discourse/t/25/1.json' do not reach the Drupal stack
  but instead receive a 404 response from the PHP Built-in server.
* If using 'drush rs' to run the server SSO between Drupal and Discourse will
  not work. Drush RS has a bug where any call to setcookie wipes out any previously
  set cookies. This prevents Drupal logins amongst other bugs.
* Discourse screens will have jQuery set to version 1.8. This should not cause any issues
  but you may see errors if your javascript code uses deprecated jQuery functions.

## Roadmap

* Performance optimisations, avoid bootstrapping entire Drupal stack on each proxy call.
