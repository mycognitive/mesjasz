<?php
// $Id: theme.inc,v 1.60 2008/09/30 20:57:38 merlinofchaos Exp $

/**
 * @file theme.inc
 *
 * An array of preprocessors to fill variables for templates and helper
 * functions to make theming easier.
 */

/**
 * Provide a full array of possible themes to try for a given hook.
 *
 * @param $hook
 *   The hook to use. This is the base theme/template name.
 * @param $view
 *   The view being rendered.
 * @param $display
 *   The display being rendered, if applicable.
 */
function _views_theme_functions($hook, $view, $display = NULL) {
  $themes = array();

  if ($display) {
    $themes[] = $hook . '__' . $view->name . '__' . $display->id;
    $themes[] = $hook . '__' . $display->id;
    if ($display->id != $display->display_plugin) {
      $themes[] = $hook . '__' . $view->name . '__' . $display->display_plugin;
      $themes[] = $hook . '__' . $display->display_plugin;
    }
  }
  $themes[] = $hook . '__' . $view->name;
  $themes[] = $hook;

  return $themes;
}

/**
 * Preprocess the primary theme implementation for a view.
 */
function template_preprocess_views_view(&$vars) {
  $view = $vars['view'];

  $vars['rows']       = !empty($view->result) || !empty($view->style_plugin->definition['even empty']) ? $view->style_plugin->render($view->result) : '';

  $vars['css_name']   = views_css_safe($view->name);
  $vars['name']       = $view->name;
  $vars['display_id'] = $view->current_display;

  if (!$vars['rows']) {
    $vars['empty']    = $view->display_handler->render_empty();
    if (!$view->display_handler->get_option('header_empty')) {
      $vars['header'] = '';
    }
    if (!$view->display_handler->get_option('footer_empty')) {
      $vars['footer'] = '';
    }
  }
  else {
    $vars['empty']    = '';
    $header = TRUE;
  }

  $vars['exposed']    = !empty($view->exposed_widgets) ? $view->exposed_widgets : '';
  if (!isset($vars['header'])) {
    $vars['header']   = $view->display_handler->render_header();
  }
  if (!isset($vars['footer'])) {
    $vars['footer']   = $view->display_handler->render_footer();
  }
  $vars['more']       = $view->display_handler->render_more_link();
  $vars['feed_icon']  = !empty($view->feed_icon) ? $view->feed_icon : '';

  $vars['attachment_before'] = !empty($view->attachment_before) ? $view->attachment_before : '';
  $vars['attachment_after'] = !empty($view->attachment_after) ? $view->attachment_after : '';

  $vars['pager']      = '';

  $exposed_input = isset($view->exposed_data_raw) ? $view->exposed_data_raw : NULL;
  if (!empty($view->pager['use_pager'])) {
    $pager_type = ($view->pager['use_pager'] === 'mini' ? 'views_mini_pager' : 'pager');
    $pager_theme = views_theme_functions($pager_type, $view, $view->display_handler->display);
    $vars['pager']    = theme($pager_theme, $exposed_input, $view->pager['items_per_page'], $view->pager['element']);
  }

  // if administrator, add some links. These used to be tabs, but this is better.
  if (user_access('administer views') && module_exists('views_ui') && empty($view->hide_admin_links) && !variable_get('views_no_hover_links', FALSE)) {
    $vars['admin_links_raw'] = array(
      array(
        'title' => t('Edit'),
        'alt' => t("Edit this view"),
        'href' => "admin/build/views/edit/$view->name",
        'fragment' => 'views-tab-' . $view->current_display,
        'query' => drupal_get_destination(),
      ),
      array(
        'title' => t('Export'),
        'alt' => t("Export this view"),
        'href' => "admin/build/views/export/$view->name",
      ),
      array(
        'title' => t('Clone'),
        'alt' => t("Create a copy of this view"),
        'href' => "admin/build/views/clone/$view->name",
      ),
    );

    drupal_alter('views_admin_links', $vars['admin_links_raw']);
    $vars['admin_links'] = theme('links', $vars['admin_links_raw']);
    views_add_css('views');
  }
  else {
    $vars['admin_links'] = '';
    $vars['admin_links_raw'] = array();
  }

  // Our JavaScript needs to have some means to find the HTML belonging to this
  // view.
  //
  // It is true that the DIV wrapper has classes denoting the name of the view
  // and its display ID, but this is not enough to unequivocally match a view
  // with its HTML, because one view may appear several times on the page. So
  // we set up a running counter, $dom_id, to issue a "unique" identifier for
  // each view. This identifier is written to both Drupal.settings and the DIV
  // wrapper.
  static $dom_id = 1;
  $vars['dom_id'] = !empty($view->dom_id) ? $view->dom_id : $dom_id++;

  // If using AJAX, send identifying data about this view.
  if ($view->use_ajax) {
    $settings = array(
      'views' => array(
        'ajax_path' => url('views/ajax'),
        'ajaxViews' => array(
          array(
            'view_name' => $view->name,
            'view_display_id' => $view->current_display,
            'view_args' => implode('/', $view->args),
            'view_path' => $_GET['q'],
            'view_dom_id' => $vars['dom_id'],
            // To fit multiple views on a page, the programmer may have
            // overridden the display's pager_element.
            'pager_element' => $view->pager['element'],
          ),
        ),
      ),
    );

    drupal_add_js($settings, 'setting');
    views_add_js('ajax_view');
  }
}

/**
 * Preprocess theme function to print a single record from a row, with fields
 */
function template_preprocess_views_view_fields(&$vars) {
  $view = $vars['view'];

  // Loop through the fields for this view.
  $inline = FALSE;
  $vars['fields'] = array(); // ensure it's at least an empty array.
  foreach ($view->field as $id => $field) {
    if (empty($field->options['exclude'])) {
      $object = new stdClass();

      $object->content = $view->field[$id]->theme($vars['row']);
      if (isset($view->field[$id]->field_alias) && isset($vars['row']->{$view->field[$id]->field_alias})) {
        $object->raw = $vars['row']->{$view->field[$id]->field_alias};
      }
      else {
        $object->raw = NULL; // make sure it exists to reduce NOTICE
      }
      $object->inline = !empty($vars['options']['inline'][$id]);
      $object->inline_html = $object->inline ? 'span' : 'div';
      if (!empty($vars['options']['separator']) && $inline && $object->inline && $object->content) {
        $object->separator = filter_xss_admin($vars['options']['separator']);
      }

      $inline = $object->inline;

      $object->handler = &$view->field[$id];
      $object->element_type = $object->handler->element_type();

      $object->class = views_css_safe($id);
      $object->label = check_plain($view->field[$id]->label());
      $vars['fields'][$id] = $object;
    }
  }

}

/**
 * Display a single views field.
 *
 * Interesting bits of info:
 * $field->field_alias says what the raw value in $row will be. Reach it like
 * this: @code { $row->{$field->field_alias} @endcode
 */
function theme_views_view_field($view, $field, $row) {
  return $field->render($row);
}

/**
 * Process a single field within a view.
 *
 * This preprocess function isn't normally run, as a function is used by
 * default, for performance. However, by creating a template, this
 * preprocess should get pickedup.
 */
function template_preprocess_views_view_field(&$vars) {
  $vars['output'] = $vars['field']->render($vars['row']);
}

/**
 * Preprocess theme function to print a single record from a row, with fields
 */
function template_preprocess_views_view_summary(&$vars) {
  $view     = $vars['view'];
  $argument = $view->argument[$view->build_info['summary_level']];

  $url_options = array();

  if (!empty($view->exposed_raw_input)) {
    $url_options['query'] = $view->exposed_raw_input;
  }
  foreach ($vars['rows'] as $id => $row) {
    $vars['rows'][$id]->link = $argument->summary_name($row);
    $args = $view->args;
    $args[$argument->position] = $argument->summary_argument($row);

    $vars['rows'][$id]->url = url($view->get_url($args), $url_options);
    $vars['rows'][$id]->count = intval($row->{$argument->count_alias});
  }
}

/**
 * Template preprocess theme function to print summary basically
 * unformatted.
 */
function template_preprocess_views_view_summary_unformatted(&$vars) {
  $view     = $vars['view'];
  $argument = $view->argument[$view->build_info['summary_level']];

  $url_options = array();

  if (!empty($view->exposed_raw_input)) {
    $url_options['query'] = $view->exposed_raw_input;
  }

  $count = 0;
  foreach ($vars['rows'] as $id => $row) {
    // only false on first time:
    if ($count++) {
      $vars['rows'][$id]->separator = filter_xss_admin($vars['options']['separator']);
    }
    $vars['rows'][$id]->link = $argument->summary_name($row);
    $args = $view->args;
    $args[$argument->position] = $argument->summary_argument($row);

    $vars['rows'][$id]->url = url($view->get_url($args), $url_options);
    $vars['rows'][$id]->count = intval($row->{$argument->count_alias});
  }
}

/**
 * Display a view as a table style.
 */
function template_preprocess_views_view_table(&$vars) {
  $view     = $vars['view'];

  // We need the raw data for this grouping, which is passed in as $vars['rows'].
  // However, the template also needs to use for the rendered fields.  We
  // therefore swap the raw data out to a new variable and reset $vars['rows']
  // so that it can get rebuilt.
  $result   = $vars['rows'];
  $vars['rows'] = array();

  $options  = $view->style_plugin->options;
  $handler  = $view->style_plugin;

  $fields   = &$view->field;
  $columns  = $handler->sanitize_columns($options['columns'], $fields);

  $active   = !empty($handler->active) ? $handler->active : '';
  $order    = !empty($handler->order) ? $handler->order : 'asc';

  $query    = tablesort_get_querystring();
  if ($query) {
    $query = '&' . $query;
  }

  foreach ($columns as $field => $column) {
    // render the header labels
    if ($field == $column && empty($fields[$field]->options['exclude'])) {
      $label = check_plain(!empty($fields[$field]) ? $fields[$field]->label() : '');
      if (empty($options['info'][$field]['sortable'])) {
        $vars['header'][$field] = $label;
      }
      else {
        // @todo -- make this a setting
        $initial = 'asc';

        if ($active == $field && $order == 'asc') {
          $initial = 'desc';
        }

        $image = theme('tablesort_indicator', $initial);
        $title = t('sort by @s', array('@s' => $label));
        $link_options = array(
          'html' => true,
          'attributes' => array('title' => $title),
          'query' => 'order=' . urlencode($field) . '&sort=' . $initial . $query,
        );
        $vars['header'][$field] = l($label . $image, $_GET['q'], $link_options);
      }
    }

    // Create a second variable so we can easily find what fields we have and what the
    // CSS classes should be.
    $vars['fields'][$field] = views_css_safe($field);
    if ($active == $field) {
      $vars['fields'][$field] .= ' active';
    }

    // Render each field into its appropriate column.
    foreach ($result as $num => $row) {
      if (!empty($fields[$field]) && empty($fields[$field]->options['exclude'])) {
        $field_output = $fields[$field]->theme($row);

        // Don't bother with separators and stuff if the field does not show up.
        if (!isset($field_output) && isset($vars['rows'][$num][$column])) {
          continue;
        }

        // Place the field into the column, along with an optional separator.
        if (isset($vars['rows'][$num][$column])) {
          if (!empty($options['info'][$column]['separator'])) {
            $vars['rows'][$num][$column] .= filter_xss_admin($options['info'][$column]['separator']);
          }
        }
        else {
          $vars['rows'][$num][$column] = '';
        }

        $vars['rows'][$num][$column] .= $field_output;
      }
    }
  }

  $vars['class'] = 'views-table';
  if (!empty($options['sticky'])) {
    drupal_add_js('misc/tableheader.js');
    $vars['class'] .= " sticky-enabled";
  }
}

/**
 * Display a view as a grid style.
 */
function template_preprocess_views_view_grid(&$vars) {
  $view     = $vars['view'];
  $result   = $view->result;
  $options  = $view->style_plugin->options;
  $handler  = $view->style_plugin;

  $columns  = $options['columns'];

  $rows = array();

  if ($options['alignment'] == 'horizontal') {
    $row = array();
    foreach ($vars['rows'] as $count => $item) {
      $row[] = $item;
      if (($count + 1) % $columns == 0) {
        $rows[] = $row;
        $row = array();
      }
    }
    if ($row) {
      $rows[] = $row;
    }
  }
  else {
    $num_rows = floor(count($vars['rows']) / $columns);
    // The remainders are the 'odd' columns that are slightly longer.
    $remainders = count($vars['rows']) % $columns;
    $row = 0;
    $col = 0;
    foreach ($vars['rows'] as $count => $item) {
      $rows[$row][$col] = $item;
      $row++;

      if (!$remainders && $row == $num_rows) {
        $row = 0;
        $col++;
      }
      else if ($remainders && $row == $num_rows + 1) {
        $row = 0;
        $col++;
        $remainders--;
      }
    }
  }
  $vars['rows'] = $rows;
}

/**
 * Display the simple view of rows one after another
 */
function template_preprocess_views_view_unformatted(&$vars) {
  $view     = $vars['view'];
  $rows     = $vars['rows'];

  $vars['classes'] = array();
  // Set up striping values.
  foreach ($rows as $id => $row) {
    $vars['classes'][$id] = 'views-row-' . ($id + 1);
    $vars['classes'][$id] .= ' views-row-' . ($id % 2 ? 'even' : 'odd');
    if ($id == 0) {
      $vars['classes'][$id] .= ' views-row-first';
    }
  }
  $vars['classes'][$id] .= ' views-row-last';
}

/**
 * Display the view as an HTML list element
 */
function template_preprocess_views_view_list(&$vars) {
  template_preprocess_views_view_unformatted($vars);
}

/**
 * Preprocess an RSS feed
 */
function template_preprocess_views_view_rss(&$vars) {
  global $base_url;
  global $language;

  $view     = &$vars['view'];
  $options  = &$vars['options'];
  $items    = &$vars['rows'];

  $style    = &$view->style_plugin;

  if (!empty($options['mission_description'])) {
    $description = variable_get('site_mission', '');
  }
  else {
    $description = $options['description'];
  }

  if ($view->display_handler->get_option('sitename_title')) {
    $title = variable_get('site_name', 'Drupal');
    if ($slogan = variable_get('site_slogan', '')) {
      $title .= ' - ' . $slogan;
    }
  }
  else {
    $title = $view->get_title();
  }

  // Figure out which display which has a path we're using for this feed. If there isn't
  // one, use the global $base_url
  $link_display_id = $view->display_handler->get_link_display();
  if ($link_display_id && !empty($view->display[$link_display_id])) {
    $path = $view->display[$link_display_id]->handler->get_path();
  }

  if ($path) {
    $path = $view->get_url(NULL, $path);
    $url_options = array('absolute' => TRUE);
    if (!empty($view->exposed_raw_input)) {
      $url_options['query'] = $view->exposed_raw_input;
    }

    // Compare the link to the default home page; if it's the default home page, just use $base_url.
    if ($path == variable_get('site_frontpage', 'node')) {
      $path = '';
    }

    $vars['link'] = check_url(url($path, $url_options));
  }

  $vars['namespaces'] = drupal_attributes($style->namespaces);
  $vars['channel'] = format_rss_channel($title, $vars['link'], $description, $items, $language->language);

  drupal_set_header('Content-Type: application/rss+xml; charset=utf-8');
}

/**
 * Default theme function for all filter forms.
 */
function template_preprocess_views_exposed_form(&$vars) {
  views_add_css('views');
  $form = &$vars['form'];

  // Put all single checkboxes together in the last spot.
  $checkboxes = '';

  if (!empty($form['q'])) {
    $vars['q'] = drupal_render($form['q']);
  }

  $vars['widgets'] = array();
  foreach ($form['#info'] as $id => $info) {
    // Set aside checkboxes.
    if (isset($form[$info['value']]['#type']) && $form[$info['value']]['#type'] == 'checkbox') {
      $checkboxes .= drupal_render($form[$info['value']]);
      continue;
    }
    $widget = new stdClass;
    // set up defaults so that there's always something there.
    $widget->label = $widget->operator = $widget->widget = NULL;

    if (!empty($info['label'])) {
      $widget->label = $info['label'];
    }
    if (!empty($info['operator'])) {
      $widget->operator = drupal_render($form[$info['operator']]);
    }
    $widget->widget = drupal_render($form[$info['value']]);
    $vars['widgets'][$id] = $widget;
  }

  // Wrap up all the checkboxes we set aside into a widget.
  if ($checkboxes) {
    $widget = new stdClass;
    // set up defaults so that there's always something there.
    $widget->label = $widget->operator = $widget->widget = NULL;
    $widget->widget = $checkboxes;
    $vars['widgets']['checkboxes'] = $widget;
  }

  // Don't render these:
  unset($form['form_id']);
  unset($form['form_build_id']);
  unset($form['form_token']);

  // This includes the submit button.
  $vars['button'] = drupal_render($form);
}

function theme_views_mini_pager($tags = array(), $limit = 10, $element = 0, $parameters = array(), $quantity = 9) {
  global $pager_page_array, $pager_total;

  // Calculate various markers within this pager piece:
  // Middle is used to "center" pages around the current page.
  $pager_middle = ceil($quantity / 2);
  // current is the page we are currently paged to
  $pager_current = $pager_page_array[$element] + 1;
  // max is the maximum page number
  $pager_max = $pager_total[$element];
  // End of marker calculations.


  $li_previous = theme('pager_previous', (isset($tags[1]) ? $tags[1] : t('‹‹')), $limit, $element, 1, $parameters);
  $li_next = theme('pager_next', (isset($tags[3]) ? $tags[3] : t('››')), $limit, $element, 1, $parameters);

  if ($pager_total[$element] > 1) {
    $items[] = array(
      'class' => 'pager-previous',
      'data' => $li_previous,
    );

    $items[] = array(
      'class' => 'pager-current',
      'data' => t('@current of @max', array('@current' => $pager_current, '@max' => $pager_max)),
    );

    $items[] = array(
      'class' => 'pager-next',
      'data' => $li_next,
    );
    return theme('item_list', $items, NULL, 'ul', array('class' => 'pager'));
  }
}

/**
 * @defgroup views_templates Views' template files
 * @{
 * All views templates can be overridden with a variety of names, using
 * the view, the display ID of the view, the display type of the view,
 * or some combination thereof.
 *
 * For each view, there will be a minimum of two templates used. The first
 * is used for all views: views-view.tpl.php.
 *
 * The second template is determined by the style selected for the view. Note
 * that certain aspects of the view can also change which style is used; for
 * example, arguments which provide a summary view might change the style to
 * one of the special summary styles.
 *
 * The default style for all views is views-view-unformatted.tpl.php
 *
 * Many styles will then farm out the actual display of each row to a row
 * style; the default row style is views-view-fields.tpl.php.
 *
 * Here is an example of all the templates that will be tried in the following
 * case:
 *
 * View, named foobar. Style: unformatted. Row style: Fields. Display: Page.
 *
 * - views-view--foobar--page.tpl.php
 * - views-view--page.tpl.php
 * - views-view--foobar.tpl.php
 * - views-view.tpl.php
 *
 * - views-view-unformatted--foobar--page.tpl.php
 * - views-view-unformatted--page.tpl.php
 * - views-view-unformatted--foobar.tpl.php
 * - views-view-unformatted.tpl.php
 *
 * - views-view-fields--foobar--page.tpl.php
 * - views-view-fields--page.tpl.php
 * - views-view-fields--foobar.tpl.php
 * - views-view-fields.tpl.php
 *
 * Important! When adding a new template to your theme, be sure to flush the
 * theme registry cache!
 *
 * @see _views_theme_functions
 * @}
 */
