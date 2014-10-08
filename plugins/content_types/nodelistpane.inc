<?php

$plugin = array(
  'single' => TRUE,
  'title' => t('Node List'),
  'description' => t('List and order custom number of nodes.'),
  'category' => t('Custom Panes'),
  'edit form' => array(
    'nodelistpane_configure_form' => array(
      'default' => TRUE,
      'title' => t('Configure list'),
    ),
    'nodelistpane_select_form' => t('Select nodes'),
  ),
  'render callback' => 'nodelistpane_render',
  'admin info' => 'nodelistpane_admin_info',
  'defaults' => array(),
  'all contexts' => TRUE,
);

/**
 * Configuration form.
 */
function nodelistpane_configure_form($form, &$form_state) {
  $conf = $form_state['conf'];

  $form['admin_title'] = array(
    '#type' => 'textfield',
    '#default_value' => isset($conf['admin_title']) ? $conf['admin_title'] : '',
    '#title' => t('Administrative title'),
    '#description' => t('This title will be used administratively to identify this pane. If blank, the regular title will be used.'),
  );

  $form['limit'] = array(
    '#type' => 'select',
    '#options' => drupal_map_assoc(range(1, NODELISTPANE_NODE_LIMIT)),
    '#title' => t('Number of nodes'),
    '#description' => t('Select the number of nodes to show in this list.'),
    '#default_value' => isset($conf['limit']) ? $conf['limit'] : 1,
  );

  $entity_info = entity_get_info('node');
  $options = array();
  if (!empty($entity_info['view modes'])) {
    foreach ($entity_info['view modes'] as $mode => $settings) {
      $options[$mode] = $settings['label'];
    }
  }
  $form['build_mode'] = array(
    '#type' => 'select',
    '#options' => $options,
    '#title' => t('Build mode'),
    '#description' => t('Select a build mode for this node.'),
    '#default_value' => isset($conf['build_mode']) ? $conf['build_mode'] : 'nodelistpane',
  );

  return $form;
}

/**
 * Configuration form submit function.
 */
function nodelistpane_configure_form_submit($form, &$form_state) {
  foreach (array('limit', 'build_mode', 'admin_title') as $key) {
    $form_state['conf'][$key] = $form_state['values'][$key];
  }
}

/**
 * Node selection form.
 *
 * @see: ctools_node_content_type_edit_form()
 */
function nodelistpane_select_form($form, &$form_state) {
  $conf = $form_state['conf'];
  // $conf['limit'] have been set in the previous step
  $limit = $conf['limit'];

  $form['nodes']['#tree'] = TRUE;

  // $conf['nodes'] is unset if we are creating a new pane. In that case we
  // initialize it as an empty array, so array_shift below won't throw a notice.
  if (isset($conf['nodes'])) {
    uasort($conf['nodes'], 'drupal_sort_weight');
  }
  else {
    $conf['nodes'] = array();
  }

  while ($limit > 0) {
    $node = array_shift($conf['nodes']);
    $form['nodes'][$limit] = array(
      'nid' => array(
        '#prefix' => '<div class="no-float">',
        '#title' => t('Enter the title or NID of a node'),
        '#type' => 'textfield',
        '#default_value' => $node['nid'],
        '#required' => TRUE,
        '#maxlength' => 512,
        '#autocomplete_path' => 'ctools/autocomplete/node',
        '#weight' => -10,
        '#element_validate' => array('nodelistpane_node_validate'),
        '#suffix' => '</div>',
      ),
      'weight' => array(
        '#type' => 'weight',
        '#title' => t('Weight'),
        '#default_value' => $node['weight'],
        '#delta' => 10,
        '#title_display' => 'invisible',
      ),
    );
    $limit--;
  }

  return $form;
}

/**
 * Node selection form submit function.
 */
function nodelistpane_select_form_submit($form, &$form_state) {
  $form_state['conf']['nodes'] = $form_state['values']['nodes'];
}

/**
 * Rendering the node list.
 *
 * @param $subtype string
 * @param $conf array
 *   Configuration as done at admin time.
 * @param $args
 * @param $context
 *   Context - in this case we don't have any.
 *
 * @return
 *   An object with at least title and content members.
 */
function nodelistpane_render($subtype, $conf, $args, $contexts) {

  $block = new stdClass();

  // Renders nodes as unordered list, respects access permissions.
  $nodes = node_load_multiple($conf['nodes']['nids']);
  $node_list = array_filter($nodes, function ($node){
    return node_access('view', $node);
  });
  $render_array = node_view_multiple($node_list, $conf['build_mode']);
  $nids = element_children($render_array['nodes']);
  $item_list = array();
  foreach ($nids as $nid) {
    $item_list[] = drupal_render($render_array['nodes'][$nid]);
  }
  $block->content = array(
    '#items' => $item_list,
    '#theme' => 'item_list',
  );

  return $block;
}

/**
 * Admin info.
 *
 * Displays the number of nodes listed, titles (or nids) for each node
 * and the selected view mode.
 */
function nodelistpane_admin_info($subtype, $conf, $contexts) {
  if (!empty($conf)) {
    // Display node titles.
    $node_titles = array();
    foreach ($conf['nodes'] as $key => $node) {
      if ($key == 'nids') {
        continue;
      } // if
      $node_titles[] = $node['nid'];
    }
    $nodes = array(
      '#theme' => 'item_list',
      '#items' => $node_titles,
    );

    $content = '<p>'
        . t('Number of nodes listed: %limit', array('%limit' => $conf['limit']))
        . '</p>' . drupal_render($nodes) . '<p>'
        . t('View mode: %build_mode', array('%build_mode' => $conf['build_mode']))
        . '</p>';

    $block = new stdClass;
    $block->title = !empty($conf['admin_title']) ? $conf['admin_title'] : t('Node list');
    $block->content = $content;
    return $block;
  }
}

/**
 * Theme callback for the nodelistpane_select_form form.
 *
 * Adds the tabledrag functionality.
 *
 * @return array
 *   The rendered tabledrag form
 */
function theme_nodelistpane_select_form($variables) {
  $form = $variables['form'];

  $rows = array();
  foreach (element_children($form['nodes']) as $id) {
    $form['nodes'][$id]['weight']['#attributes']['class'] = array('node-weight');
    $rows[] = array(
      'data' => array(
        drupal_render($form['nodes'][$id]['nid']),
        drupal_render($form['nodes'][$id]['weight']),
      ),
      'class' => array('draggable'),
    );
  }
  $header = array(t('Node Title'), t('Weight'));

  $table_id = 'node-list-table';
  $output = theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('id' => $table_id),
  ));
  // Render any remaining form elements (such as submit button).
  $output .= drupal_render_children($form);
  drupal_add_tabledrag($table_id, 'order', 'sibling', 'node-weight');
  return $output;
}

/**
 * Form validation for the node selection.
 *
 * @see: ctools_node_content_type_edit_form_validate()
 */
function nodelistpane_node_validate($element, &$form_state) {
  $nid = $element['#value'];
  $preg_matches = array();
  $match = preg_match('/\[id: (\d+)\]/', $nid, $preg_matches);
  if (!$match) {
    $match = preg_match('/^id: (\d+)/', $nid, $preg_matches);
  }

  if ($match) {
    $nid = $preg_matches[1];
  }
  if (is_numeric($nid)) {
    $node = db_query('SELECT nid, status FROM {node} WHERE nid = :nid', array(':nid' => $nid))->fetchObject();
  }
  else {
    $node = db_query('SELECT nid, status FROM {node} WHERE LOWER(title) = LOWER(:title)', array(':title' => $nid))->fetchObject();
  }
  if ($node) {
    $form_state['values']['nodes']['nids'][] = $node->nid;
  }

  if (!($node || preg_match('/^[@%]\d+$/', $nid)) ||
      // Do not allow unpublished nodes to be selected by unprivileged users
      (empty($node->status) && !user_access('administer nodes'))) {
    form_error($element, t('Invalid node'));
  }
}