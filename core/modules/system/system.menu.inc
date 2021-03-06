<?php

/**
 * Implements hook_block_configure().
 */
function _system_block_configure($delta = '', $settings = array()) {
  // Merge in the default configuration.
  $settings += system_menu_block_defaults($delta);
  return system_menu_block_form($settings);
}

/**
 * The default menu block configuration.
 *
 * @return array
 */
function system_menu_block_defaults($menu_name) {
  $defaults = array(
    'level' => 1,
    'menu_name' => $menu_name,
  );
  if ($menu_name == 'main-menu') {
    $defaults['depth'] = 1;
  }
  else {
    $defaults['depth'] = 0;
  }
  return $defaults;
}

/**
 * Returns the configuration form for a menu tree.
 *
 * @param $config
 *   An associated array of settings passed in by hook_block_configure(). 
 *   If none are given, default configuration is assumed.
 * @return
 *   The form array. This is a form fragment and not in full Form API format.
 */
function system_menu_block_form($config) {
  $form['menu_name'] = array(
    '#type' => 'value',
    '#value' => $config['menu_name'],
  );
  $form['level'] = array(
    '#type' => 'select',
    '#title' => t('Starting level'),
    '#default_value' => $config['level'],
    '#options' => array(
      '1' => t('1st level (primary)'),
      '2' => t('2nd level (secondary)'),
      '3' => t('3rd level (tertiary)'),
      '4' => t('4th level'),
      '5' => t('5th level'),
      '6' => t('6th level'),
      '7' => t('7th level'),
      '8' => t('8th level'),
      '9' => t('9th level'),
    ),
    '#description' => t('Blocks that start with the 1st level will always be visible. Blocks that start with the 2nd level or deeper will only be visible when the trail to the active menu item passes though the block’s starting level.'),
  );
  $form['depth'] = array(
    '#type' => 'select',
    '#title' => t('Maximum depth'),
    '#default_value' => $config['depth'],
    '#options' => array(
      '1' => '1',
      '2' => '2',
      '3' => '3',
      '4' => '4',
      '5' => '5',
      '6' => '6',
      '7' => '7',
      '8' => '8',
      '9' => '9',
      '0' => t('Unlimited'),
    ),
    '#description' => t('From the starting level, specify the maximum depth of the menu tree.'),
  );

  return $form;
}

/**
 * Build a menu tree based on the provided configuration.
 *
 * @param array $config
 *   An array of configuration options that specifies how to build the
 *   menu tree and its title.
 *   - menu_name: (string) The machine name of the requested menu. Can also be
 *     set to MENU_TREE__CURRENT_PAGE_MENU to use the menu selected by the page.
 *   - level: (int) The starting level of the tree.
 *   - depth: (int) The maximum depth the tree should contain, relative to the
 *     starting level.
 *
 * @return array
 *   An associative array containing several pieces of data.
 *   - content: The tree as a renderable array.
 *   - subject: The title rendered as HTML.
 *   - subject_array: The title as a renderable array.
 */
function system_menu_block_build(array $config) {
  if (module_exists('menu')) {
    $menu_names = menu_get_menus();
  }
  else {
    $menu_names = menu_list_system_menus();
  }  
  
  // Get the default block name.
  backdrop_static_reset('menu_block_set_title');
  $menu_name = str_replace('_', '-', $config['menu_name']);
  system_menu_block_set_title($menu_names[$menu_name]);

  // Get the raw menu tree data.
  $tree = system_menu_tree_block_data($config);
  $title = system_menu_block_get_title();
  
  // Create a renderable tree.
  $data = array();
  $data['subject'] = $title;
  $data['content'] = array();
  if (!empty($tree) && $output = menu_tree_output($tree)) {
    $data['content'] = $output;
  }

  return $data;
}

/**
 * Gets the data structure representing a menu tree for the given configuration.
 *
 * @param array $config
 *   See the $config param of menu_tree_build().
 *
 * @return array
 */
function system_menu_tree_block_data(array &$config) {
  // Determine the max depth based on level and depth setting.
  $max_depth = ($config['depth'] == 0) ? NULL : min($config['level'] + $config['depth'] - 1, MENU_MAX_DEPTH);

  // Get the tree pruned for just the active trail.
  $tree = menu_tree_page_data($config['menu_name'], $max_depth);

  // Allow alteration of the tree and config before we begin operations on it.
  backdrop_alter('menu_block_tree', $tree, $config);

  // Prune the tree along the active trail to the specified level.
  if ($config['level'] > 1) {
    system_menu_tree_prune_tree($tree, $config['level']);
  }

  // Trim the tree to the active path.
  system_menu_tree_trim_active_path($tree);

  // Trim the branches that extend beyond the specified depth.
  if ($config['depth'] > 0) {
    system_menu_tree_depth_trim($tree, $config['depth']);
  }

  return $tree;
}

/**
 * Prune a tree so that it begins at the specified level.
 *
 * This function will follow the active menu trail to the specified level.
 *
 * @param array $tree
 *   The menu tree to prune.
 * @param int $level
 *   The level of the original tree that will start the pruned tree.
 * @param array $parent_item
 *   The menu item that should be used as the root of the tree.
 */
function system_menu_tree_prune_tree(array &$tree, $level, array $parent_item = NULL) {
  if (!empty($parent_item)) {
    // Prune the tree along the path to the menu item.
    for ($i = 1; $i <= MENU_MAX_DEPTH && $parent_item["p$i"] != '0'; $i++) {
      $plid = $parent_item["p$i"];
      $found_active_trail = FALSE;
      // Examine each element at this level for the ancestor.
      foreach ($tree as $key => &$value) {
        if ($tree[$key]['link']['mlid'] == $plid) {
          system_menu_block_set_title($tree[$key]['link']);
          // Prune the tree to the children of this ancestor.
          $tree = $tree[$key]['below'] ? $tree[$key]['below'] : array();
          $found_active_trail = TRUE;
          break;
        }
      }
      // If we don't find the ancestor, bail out.
      if (!$found_active_trail) {
        $tree = array();
        break;
      }
    }
  }

  $is_front_page = backdrop_is_front_page();
  // Trim the upper levels down to the one desired.
  for ($i = 1; $i < $level; $i++) {
    $found_active_trail = FALSE;
    // Examine each element at this level for the active trail.
    foreach ($tree as $key => &$value) {
      // Also include the children of the front page.
      if ($tree[$key]['link']['in_active_trail'] || ($tree[$key]['link']['link_path'] == '<front>' && $is_front_page)) {
        // Get the title for the pruned tree.
        system_menu_block_set_title($tree[$key]['link']);
        // Prune the tree to the children of the item in the active trail.
        $tree = $tree[$key]['below'] ? $tree[$key]['below'] : array();
        $found_active_trail = TRUE;
        break;
      }
    }
    // If we don't find the active trail, the active item isn't in the tree we want.
    if (!$found_active_trail) {
      $tree = array();
      break;
    }
  }
}

/**
 * Trim everything but the active trail in the tree.
 *
 * @param array $tree
 *   The menu tree to trim.
 */
function system_menu_tree_trim_active_path(array &$tree) {
  foreach ($tree as $key => &$value) {
    if (($tree[$key]['link']['in_active_trail'] || $tree[$key]['link']['expanded']) && $tree[$key]['below']) {
      // Continue in the subtree, if it exists.
      system_menu_tree_trim_active_path($tree[$key]['below']);
    }
    else {
      // Trim anything not expanded or along the active trail.
      $tree[$key]['below'] = FALSE;
    }
  }
}

/**
 * Prune a tree so it does not extend beyond the specified depth limit.
 *
 * @param array $tree
 *   The menu tree to prune.
 * @param int $depth_limit
 *   The maximum depth of the returned tree; must be a positive integer.
 */
function system_menu_tree_depth_trim(array &$tree, $depth_limit) {
  // Prevent invalid input from returning a trimmed tree.
  if ($depth_limit < 1) {
    return;
  }

  // Examine each element at this level to find any possible children.
  foreach ($tree as $key => &$value) {
    if ($tree[$key]['below']) {
      if ($depth_limit > 1) {
        system_menu_tree_depth_trim($tree[$key]['below'], $depth_limit-1);
      }
      else {
        // Remove the children items.
        $tree[$key]['below'] = FALSE;
      }
    }
    if ($depth_limit == 1 && $tree[$key]['link']['has_children']) {
      // Turn off the menu styling that shows there were children.
      $tree[$key]['link']['has_children'] = FALSE;
      $tree[$key]['link']['leaf_has_children'] = TRUE;
    }
  }
}


/**
 * Sets the menu item to use for the tree's title.
 *
 * @param array|string $item
 *   The menu item (an array) or the menu item's title as a string.
 *
 * @return array|string
 *   The saved title value.
 */
function system_menu_block_set_title($item = NULL) {
  $menu_item = &backdrop_static(__FUNCTION__, '');
  // Save the menu item.
  if (!is_null($item)) {
    $menu_item = $item;
  }

  return $menu_item;
}

/**
 * Retrieves the menu item to use for the tree's title.
 *
 * @return array|string
 *   A render array or string containing the tree's title.
 */
function system_menu_block_get_title() {
  $menu_item = system_menu_block_set_title();

  if (is_string($menu_item)) {
    // The tree's title is a menu title, a normal string.
    $title = check_plain($menu_item);
  }
  // The tree's title is a menu item.
  else {
    $title = check_plain($menu_item['title']);
  }
  return $title;
}
