<?php

/**
 * @file Handle SSO authentication for Discourse.
 */

function discourse_sso() {
    global $user;
    $payload = isset($_GET['sso']) ? $_GET['sso'] : NULL;
    $sig = isset($_GET['sig']) ? $_GET['sig'] : NULL;
    if (!$payload) {
        $payload = isset($_SESSION['discourse_sso_payload']) ? $_SESSION['discourse_sso_payload'] : NULL;
    }
    if (!$sig) {
        $sig = isset($_SESSION['discourse_sso_sig']) ? $_SESSION['discourse_sso_sig'] : NULL;
    }
    if (!$payload || !$sig) {
        drupal_access_denied();
    }
    if ($user->uid) {
        discourse_sso_validate($payload, $sig);
    } else {
        discourse_sso_login($payload, $sig);
    }
}


function discourse_sso_login($payload, $sig) {
    $_SESSION['discourse_sso_sig'] = $sig;
    $_SESSION['discourse_sso_payload'] = $payload;
    // TODO add 10 minute timeout and catch registrations
    drupal_goto('user', array('query' => array('destination' => 'discourse/sso')));
}

function discourse_sso_validate($payload, $sig) {
    global $user;

    if (preg_match('/[^\x20-\x7f]/', $user->name)) {
      $username = $user->mail;
    } else {
      $username = $user->name;
    };

    $payload = urldecode($payload);
    if (!hash_hmac("sha256", $payload, variable_get('discourse_sso_secret')) === $sig) {
        drupal_access_denied();
    }
    $query = array();
    parse_str(base64_decode($payload), $query);
    $nonce = isset($query["nonce"]) ? $query['nonce'] : NULL;
    if (!$nonce) {
        drupal_access_denied();
    }
    $return_payload = base64_encode(http_build_query(array(
        'external_id' => $user->uid,
        'email'       => $user->mail,
        'nonce'       => $nonce,
        'username'    => $username,
        'name'        => $user->name,
    )));
    $return_sig = hash_hmac("sha256", $return_payload, variable_get('discourse_sso_secret'));
    drupal_goto(variable_get('discourse_server') . '/session/sso_login', array('query' => array("sso" => $return_payload, "sig" => $return_sig)));
}
