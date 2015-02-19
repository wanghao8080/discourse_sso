<?php

/**
 * @file
 * Provide the Discourse module admin page.
 */


/**
 * Discourse settings page callback.
 */
function discourse_admin_form($form, &$form_state) {
  $form['discourse_server'] = array(
    '#type' => 'textfield',
    '#title' => t('Discourse URL'),
    '#description' => t('The web address of the Discourse server.'),
    '#size' => 40,
    '#default_value' => variable_get('discourse_server'),
    '#maxlength' => 120,
    '#required' => TRUE,
  );
  /*
  $form['discourse_api_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Api Key'),
    '#description' => t('The api key for the Discourse server.'),
    '#size' => 40,
    '#default_value' => variable_get('discourse_api_key'),
    '#maxlength' => 120,
    '#required' => FALSE,
  );
  $form['discourse_api_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Discourse Admin Username'),
    '#description' => t('The admin username for the Discourse server.'),
    '#size' => 40,
    '#default_value' => variable_get('discourse_api_username'),
    '#maxlength' => 120,
    '#required' => FALSE,
  );*/
  $form['discourse_forum_root'] = array(
      '#type' => 'textfield',
      '#title' => t('Forum Root'),
      '#description' => t('The path where the Discourse forum should be served from.'),
      '#default_value' => variable_get('discourse_forum_root'),
  );
  $form['discourse_sso_secret'] = array(
      '#type' => 'textfield',
      '#title' => t('SSO Secret'),
      '#description' => t('Secret string used to encrypt/decrypt SSO information, be sure it is 10 chars or longer and matches the Discourse setting under Admin > Settings > Login. If there is a value here Drupal will attempt to login the user to Discourse on Drupal login.'),
      '#default_value' => variable_get('discourse_sso_secret'),
  );
  $form['#validate'][] = 'discourse_admin_form_validate';
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );


  return $form;
}


/**
 * Save configuration settings for the Discourse module.
 */
function discourse_admin_form_submit($form, &$form_state) {
  $clear_menu_caches = (variable_get('discourse_forum_root') != $form_state['values']['discourse_forum_root']);
  variable_set('discourse_server', $form_state['values']['discourse_server']);
  //variable_set('discourse_api_key', $form_state['values']['discourse_api_key']);
  //variable_set('discourse_api_username', $form_state['values']['discourse_api_username']);
  variable_set('discourse_forum_root', $form_state['values']['discourse_forum_root']);
  variable_set('discourse_sso_secret', $form_state['values']['discourse_sso_secret']);
  if ($clear_menu_caches) {
    menu_rebuild();
  }
  drupal_set_message(t('The settings have been saved'));
}


/**
 * Validation for the admin form.
 */
function discourse_admin_form_validate($form, &$form_state) {
  $url = $form_state['values']['discourse_server'];
  if (!valid_url($url, FALSE)) {
    form_set_error('discourse_server', t('Sorry, that does not appear to be a valid url.'));
  }
  // Ensure we have the http/https prefix.
  if (substr($url, 0, 4) != 'http') {
    $form_state['values']['discourse_server'] = 'http://' . $url;
  }
  // Remove any trailing slash.
  if (substr($form_state['values']['discourse_server'], -1) == '/') {
    $form_state['values']['discourse_server'] = substr($form_state['values']['discourse_server'], 0, -1);
  }
}
