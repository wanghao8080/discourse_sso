<?php

/**
 * @file
 *
 * Provide sync between Drupal and Discourse users.
 *
 * This will require Drupal to have access to the Discourse Postgres database.
 */

// @todo check when Drupal email is updated and update Discourse email

/**
 * Log in the current user to Discourse.
 *
 */
function discourse_login() {
  global $user;
  if (!$user->uid || !$user->mail) {
    watchdog('discourse', 'Login error. No user.', array(), WATCHDOG_ERROR);
    return;
  }

  // Make request to Discourse SSO endpoint.
  $initial_sso_request = drupal_http_request(
      variable_get('discourse_server') . '/session/sso?',
      array('max_redirects' => 0)
  );
  if (isset($initial_sso_request->redirect_url)) {
    $sso_url = drupal_parse_url($initial_sso_request->redirect_url);
  } else {
    watchdog('discourse', 'Login error. No redirect url.', array(), WATCHDOG_ERROR);
    return;
  }

  // Parse Discourse SSO values.
  $encoded_payload = isset($sso_url['query']['sso'])? $sso_url['query']['sso'] : NULL;
  $sig = isset($sso_url['query']['sig'])? $sso_url['query']['sig'] : NULL;
  if (!$encoded_payload || !$sig) {
    watchdog('discourse', 'Login error. No payload or sig.', array(), WATCHDOG_ERROR);
    return;
  }
  $payload = urldecode($encoded_payload);
  if (hash_hmac("sha256", $payload, variable_get('discourse_sso_secret')) != $sig) {
    watchdog('discourse', 'Login error. Hmac invalid.', array(), WATCHDOG_ERROR);
    return;
  };

  // Calculate return values.
  $parsed_payload = array();
  parse_str(base64_decode($payload), $parsed_payload);
  $nonce = isset($parsed_payload["nonce"]) ? $parsed_payload['nonce'] : NULL;
  //$return_sso_url = isset($parsed_payload["return_sso_url"]) ? $parsed_payload['return_sso_url'] : NULL;
  $return_sso_url = variable_get('discourse_server') . '/session/sso_login';
  if (!$nonce) {
    watchdog('discourse', 'Login error. No nonce.', array(), WATCHDOG_ERROR);
    return;
  }


  if (preg_match('/[^\x20-\x7f]/', $user->name)) {
    $username = $user->mail;
  } else {
    $username = $user->name;
  };

  $return_payload = base64_encode(http_build_query(array(
      'external_id' => $user->uid,
      'email' => $user->mail,
      'name'  => $user->name,
      'username' => $username,
      'nonce' => $nonce,
  )));


  $return_sig = hash_hmac("sha256", $return_payload, variable_get('discourse_sso_secret'));

  // Send calculated SSO values back to Discourse.
  $return_sso_request = drupal_http_request(
      $return_sso_url . '?' . http_build_query(array('sso' => $return_payload, 'sig' => $return_sig)),
      array('max_redirects' => 0)
  );

  // Get cookies from response and set them on the user's browser.
  if ($return_sso_request->code == 302 && isset($return_sso_request->headers)) {
    $cookies = discourse_cookie_parse($return_sso_request->headers);
    if (isset($cookies['_t'])) {
      // Set the Discourse user token cookie to login.
      setcookie('_t', $cookies['_t'], NULL, '/', NULL, NULL, TRUE);
      // Clear the Discourse session cookie to avoid a conflict in CurrentUser->current_user
      setcookie('_forum_session', '', 0, '/', NULL, NULL, TRUE);
    } else {
      watchdog('discourse', 'Login error. No token cookie.', array(), WATCHDOG_ERROR);
    }
  } else {
    watchdog('discourse', 'Login error. No redirect.', array(), WATCHDOG_ERROR);
  }

}


/**
 * Log out the current user from Discourse.
 */
function discourse_logout() {
  setcookie('_t', '', 0, '/', NULL, NULL, TRUE);
  setcookie('_forum_session', '', 0, '/', NULL, NULL, TRUE);
}


/*
 * Cookie parsing utility.
 *
 * @params array Header array from drupal_http_request().
 * @return array Array of cookies.
 */
function discourse_cookie_parse($header) {
  $cookies = array();
  if (isset($header['set-cookie'])) {
    $line = $header['set-cookie'];
    $c_split = explode(';', $line);
    foreach ($c_split as $data) {
      $c_info = explode('=', $data);
      $c_info[0] = trim($c_info[0]);
      if (!in_array($c_info[0], array('domain', 'expires', 'path', 'secure', 'comment', 'HttpOnly'))) {
        $cookies[$c_info[0]] = $c_info[1];
      }
    }
    return $cookies;
  }
}
