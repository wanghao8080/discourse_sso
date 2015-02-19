<?php

/**
 * @file Provides placeholder blocks for Discourse elements.
 */


/**
 * Implements hook_block_info().
 * @return array
 */
function discourse_block_info() {
  $blocks['header'] = array(
    'info' => t('Discourse Header'),
    'cache' => DRUPAL_CACHE_GLOBAL,
    'pages' => 'discourse\ndiscourse/*',
  );
  return $blocks;
}


/**
 * Implements hook_block_view().
 *
 * @param string
 * @return array
 */
function discourse_block_view($delta = '') {
  $block = array();
  switch ($delta) {
    case 'header':
      $block['content'] = '<div id="discourse-header-block"></div>';
      break;
  }
  return $block;
}