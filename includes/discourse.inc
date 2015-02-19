<?php

/**
 * @file
 * Provide a page callback for the initial request for any Discourse page.
 *
 * For HTML requests;
 * JS and CSS files from the Discourse page are added to the Drupal page.
 * HTML is parsed and the main content is passed to Drupal as the content.
 * Discourse module js and css files are added to the Drupal page.
 * Extra HTML elements required by Discourse are added to the page.
 * Parsed HTML is rewritten to fix links and image sources.
 *
 * For AJAX requests;
 * The callback sends the request on to the Discourse server with cookies
 * and any POST/PUT data, and returns the response with any cookies or
 * headers required by the client side Discourse app.
 */


/**
 * Callback for all Discourse requests.
 *
 * HTML requests are handled in this function.
 * Ajax and non-get requests are passed on to discourse_ajax().
 */
function discourse_html() {
  drupal_page_is_cacheable(FALSE);
  // We need to use request_uri so we can keep periods in the querystring.
  $path = discourse_get_discourse_path();
  $discourse_server = variable_get('discourse_server');
  $url = $discourse_server . $path;
  $root_url = base_path() . variable_get('discourse_forum_root');
  // Check if this is a request for an html page.
  if ($_SERVER['REQUEST_METHOD'] != 'GET'
    || (isset($_SERVER['HTTP_X_REQUESTED_WITH']) && $_SERVER['HTTP_X_REQUESTED_WITH'] == 'XMLHttpRequest')
  ) {
    discourse_ajax($path);
  }
  // Make a request to the Discourse server.
  $options = array();
  discourse_prepare_request($url, $options);
  $response = discourse_http_request($url, $options);
  // Check for a redirect.
  if (isset($response->redirect_url)) {
    global $base_url;
    $redirect_url = str_replace($discourse_server, $base_url . $root_url, $response->redirect_url);
    drupal_goto($redirect_url, array(), $response->code);
  }
  // Check if the Discourse server is online.
  if (!isset($response->data) || !$response->data) {
    return t('Discourse server not available.');
  }
  if (in_array(arg(1), array('uploads', 'user_avatar', 'letter_avatar'))) {
    discourse_binary_response($response);
  }
  // Pass through any cookies from Discourse to the client.
  discourse_set_cookies($response);
  // Parse response for main html section.
  $html = htmlqp($response->data, 'section#main')->html();
  $html = discourse_rewrite_html($html);
  // Parse the hidden login form.
  $hidden_login_form = htmlqp($response->data, '#hidden-login-form');
  $hidden_login_form->attr('action', $root_url . '/login');
  $html .= $hidden_login_form->html();
  // Add webfont css.
  drupal_add_css(discourse_webfont_css(), array('type' => 'inline'));
  // Parse html response for js and css files.
  $js_files = htmlqp($response->data, 'script')->toArray();
  $css_files = htmlqp($response->data, 'link[rel="stylesheet"]')->toArray();
  $csrf_token = htmlqp($response->data, 'meta[name=csrf-token]')->html();
  drupal_add_html_head(array('#markup' => $csrf_token, '#type' => 'markup'), 'discourse_csrf');
  foreach ($css_files as $css) {
    $href = $css->getAttribute('href');
    // Some external files may be CDN hosted.
    if (strpos($href, '/') == 0) {
      drupal_add_css($discourse_server . $href, 'external');
    }
    else {
      drupal_add_css($href, 'external');
    }
  }
  drupal_add_css(drupal_get_path('module', 'discourse') . '/discourse.css');
  drupal_add_js('$ = jQuery;', 'inline');
  foreach ($js_files as $js) {
    if ($src = $js->getAttribute('src')) {
      if (strpos($src, 'jquery-1.8.2') == FALSE) {
        // Some external files may be CDN hosted.
        if (strpos($src, '/') == 0) {
          drupal_add_js($discourse_server . $src, 'external');
        }
        else {
          drupal_add_js($src, 'external');
        }
      }
    }
    else {
      $js_string = $js->nodeValue;
      $js_string = trim(str_replace('&#10;', "\n", $js_string));
      if (strpos($js_string, 'PreloadStore.store') !== FALSE) {
        $js_string = discourse_rewrite_preload_store($js_string);
      }
      drupal_add_js($js_string, array('type' => 'inline', 'scope' => 'footer'));
    }
  }
  drupal_add_js(drupal_get_path('module', 'discourse') . '/discourse.js');
  drupal_add_js(array(
    'discourse' => array(
      'rootURL' => $root_url,
      'server' => variable_get('discourse_server'),
      'blocks' => variable_get('discourse_header_as_block'),
    ),
  ), 'setting');
  return $html;
}


/**
 * Rewrite html to fix links in noscript area for SEO.
 *
 * @param string $html
 *   An html string as received from the Discourse server.
 *
 * @return string
 *   Html string rewritten with links and image src corrected.
 */
function discourse_rewrite_html($html) {
  $root = base_path() . variable_get('discourse_forum_root');
  $server = variable_get('discourse_server');
  $html = str_replace("href='/", "href='" . $root . '/', $html);
  $html = str_replace('href="/', 'href="' . $root . '/', $html);
  $html = str_replace('src="/', 'src="' . $server . '/', $html);
  $html = str_replace("src='/", "src='" . $server . '/', $html);
  return $html;
}


/**
 * Rewrite inline css to use correct source for images.
 *
 * @param string $css
 *   Inline css from the Discourse response.
 *
 * @return string
 *   Rewritten css with urls corrected.
 */
function discourse_rewrite_css($css) {
  $css = str_replace('&#10;', "\n", $css);
  $css = str_replace("url('/", "url('" . variable_get('discourse_server') . '/', $css);
  return $css;
}


/**
 * Rewrite the preload store settings in the inital html load.
 *
 * @param $string
 * @return string
 */
function discourse_rewrite_preload_store($string) {
  $discourse_server = variable_get('discourse_server');
  $string = str_replace('"logo_url":"/', '"logo_url":"' . $discourse_server . '/', $string);
  $string = str_replace('"logo_small_url":"/', '"logo_small_url":"' . $discourse_server . '/', $string);
  return $string;
}


/**
 * Callback to handle Discourse ajax requests.
 *
 * @param string $path
 *   The path of the url to request from the Discourse server.
 */
function discourse_ajax($path) {
  $response = discourse_ajax_response($path);
  if ($response->code == 200) {
    if (strpos($response->data, '"url":"/uploads')) {
      $response->data = str_replace('"url":"/uploads', '"url":"' . base_path() . variable_get('discourse_forum_root') . '/uploads', $response->data);
    }
  }
  echo $response->data;
  exit;
}

/**
 * Callback to handle Discourse files.
 *
 * @param string $path
 *   The path of the url to request from the Discourse server.
 */
function discourse_binary_response($response) {
  discourse_set_headers($response);
  echo $response->data;
  exit;
}


function discourse_ajax_response($path) {
  $url = variable_get('discourse_server') . $path;
  $options = array();
  discourse_prepare_request($url, $options);
  $response = discourse_http_request($url, $options);
  discourse_set_headers($response);
  return $response;
}


/**
 * Get the matching Discourse path given a Drupal path.
 *
 * @param $path string
 *     The path requested from the Drupal server.
 * @return mixed
 *     The matching path on the Discourse server.
 */
function discourse_get_discourse_path() {
  // TODO use regexp to just replace the first instance.
  $path = str_replace(base_path() . variable_get('discourse_forum_root'), '', '/' . $_GET['q']);
  if ($querystring = discourse_get()) {
    $path .= '?' . http_build_query($querystring);
  };
  return $path;
}


/**
 * Prepare a request to be passed on to the Discourse server.
 *
 * Add cookies, correct query strings, format post data etc.
 *
 * @param string $url
 *   The url without query string for the Discourse request.
 * @param array $options
 *   The options array for discourse_http_request().
 */
function discourse_prepare_request(&$url, &$options) {
  $options = array(
      'headers' => discourse_get_all_headers(),
      'method' => $_SERVER['REQUEST_METHOD'],
      'data' => NULL,
      'max_redirects' => 0,
  );
  switch (TRUE) {
    case isset($_FILES) && count($_FILES):
      $options['data'] = multipart_encode_file_data();
      $options['headers']['Content-Length'] = strlen($options['data']);
      break;
    case $_SERVER['REQUEST_METHOD'] == 'POST':
      $options['data'] = http_build_query($_POST);
      break;
    case $_SERVER['REQUEST_METHOD'] == 'DELETE':
    case $_SERVER['REQUEST_METHOD'] == 'PUT':
      $options['data'] = file_get_contents('php://input');
      break;
  }
  // Connection: keep-alive will not work with PHP stream_socket_client
  unset($options['headers']['Connection']);
}

function multipart_encode_file_data() {
  $b_pos = strpos($_SERVER['CONTENT_TYPE'],'boundary=');
  $boundary = substr($_SERVER['CONTENT_TYPE'], $b_pos + 9);
  $output = '';
  foreach ($_POST as $key => $value) {
    $output .= "--$boundary\r\n";
    $output .= multipart_enc_text($key, $value);
  }
  foreach ($_FILES as $key => $file_data) {
    foreach ($file_data['tmp_name'] as $index => $tmp_name) {
      $output .= "--$boundary\r\n";
      $output .= multipart_enc_file($key, $index, $tmp_name, $file_data['name'], $file_data['type'], $boundary);
      $output .= "\r\n";
    }
  }
  $output .="--$boundary--";
  return $output;
}


function multipart_enc_file($key, $index, $tmp_name, $names, $types, $boundary){
  if (substr($tmp_name, 0, 1) == "@") {
    $tmp_name = substr($tmp_name, 1);
  }
  $data = "Content-Disposition: form-data; name=\"{$key}[]\"; filename=\"$names[$index]\"\r\n";
  $data .= "Content-Type: $types[$index]\r\n\r\n";
  $data .= file_get_contents($tmp_name);
  return $data;
}


function multipart_enc_text($name, $value){
  return "Content-Disposition: form-data; name=\"$name\"\r\n\r\n$value\r\n";
}




/**
 * Return an array of HTTP request headers.
 *
 * getallheaders() is only available when running Apache
 * so we need to do this ourselves.
 *
 * @return array
 *   An associative array of all the HTTP headers in the current request.
 */
function discourse_get_all_headers() {
  $headers = '';
  foreach ($_SERVER as $name => $value) {
    if (substr($name, 0, 5) == 'HTTP_') {
      $headers[str_replace(' ', '-', ucwords(strtolower(str_replace('_', ' ', substr($name, 5)))))] = $value;
    }
  }
  return $headers;
}


/**
 * Handle cookies sent from the Discourse server.
 *
 * @param object $response
 *   The response object returned by discourse_http_request().
 */
function discourse_set_cookies($response) {
  if (isset($response->headers['set-cookie'])) {
    header('Set-Cookie: ' . $response->headers['set-cookie']);
  }
}

/**
 * Handle headers sent from the Discourse server.
 *
 * @param object $response
 *   The response object returned by discourse_http_request().
 */
function discourse_set_headers($response) {
  foreach ($response->headers as $key => $value) {
    drupal_add_http_header($key, $value);
  }
}


/**
 * Replicate Discourse's webfont css.
 * Webfonts need to be served from the drupal domain to get around Firefox's
 * cross-site font restriction.
 *
 * @return string
 */
function discourse_webfont_css() {
  $fonts_root = base_path() . drupal_get_path('module', 'discourse') . '/lib/fonts';
  return <<<EOF
  @font-face {
    font-family: 'FontAwesome';
    src: url('{$fonts_root}/fontawesome-webfont.eot');
    src: url('{$fonts_root}/fontawesome-webfont.eot#iefix') format('embedded-opentype'),
      url('{$fonts_root}/fontawesome-webfont.woff') format('woff'),
      url('{$fonts_root}/fontawesome-webfont.ttf') format('truetype');
    font-weight: normal;
    font-style: normal;
  }

  @font-face {
        font-family: 'zocial';
        src: url('{$fonts_root}/zocial-regular-webfont.woff') format('woff'),
            url('{$fonts_root}/zocial-regular-webfont.ttf') format('truetype'),
            url('{$fonts_root}/zocial-regular-webfont.svg#zocialregular') format('svg');
        font-weight: normal;
        font-style: normal;
  }
EOF;
}
