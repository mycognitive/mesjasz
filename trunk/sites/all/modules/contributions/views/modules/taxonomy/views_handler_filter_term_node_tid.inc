<?php
// $Id: views_handler_filter_term_node_tid.inc,v 1.2 2008/09/22 18:13:51 merlinofchaos Exp $

/**
 * Filter by term id
 */
class views_handler_filter_term_node_tid extends views_handler_filter_many_to_one {
  function has_extra_options() { return TRUE; }

  function get_value_options() { /* don't overwrite the value options */ }

  function option_definition() {
    $options = parent::option_definition();

    $options['type'] = array('default' => 'textfield');
    $options['vid'] = array('default' => 0);

    return $options;
  }

  function extra_options_form(&$form, &$form_state) {
    $vocabularies = taxonomy_get_vocabularies();
    foreach ($vocabularies as $voc) {
      $options[$voc->vid] = check_plain($voc->name);
    }

    // We only do this when the form is displayed so this query doesn't run
    // unnecessarily just when the object is constructed.
    if ($this->options['vid'] == 0) {
      $this->options['vid'] = db_result(db_query('SELECT MIN(vid) FROM {vocabulary} v'));
    }

    $form['vid'] = array(
      '#prefix' => '<div class="views-left-40">',
      '#suffix' => '</div>',
      '#type' => 'radios',
      '#title' => t('Vocabulary'),
      '#options' => $options,
      '#description' => t('Select which vocabulary to show terms for in the regular options.'),
      '#default_value' => $this->options['vid'],
    );

    $form['markup_start'] = array(
      '#value' => '<div class="views-left-40">',
    );

    $form['type'] = array(
      '#type' => 'radios',
      '#title' => t('Selection type'),
      '#options' => array('select' => t('Dropdown'), 'textfield' => t('Autocomplete')),
      '#default_value' => $this->options['type'],
    );

    $form['hierarchy'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show hierarchy in dropdown'),
      '#default_value' => !empty($this->options['hierarchy']),
      '#process' => array('views_process_dependency'),
      '#dependency' => array('radio:options[type]' => array('select')),
    );

    $form['markup_end'] = array(
      '#value' => '</div>',
    );
  }

  function value_form(&$form, &$form_state) {
    $vocabulary = taxonomy_vocabulary_load($this->options['vid']);
    if (!$vocabulary) {
      $form['markup'] = array(
        '#prefix' => '<div class="form-item">',
        '#suffix' => '</div>',
        '#value' => t('An invalid vocabulary is selected. Please change it in the options.'),
      );
      return;
    }

    if ($this->options['type'] == 'textfield') {
      $default = '';
      if ($this->value) {
        $result = db_query("SELECT * FROM {term_data} td WHERE td.tid IN (" . implode(', ', $this->value) . ')');
        while ($term = db_fetch_object($result)) {
          if ($default) {
            $default .= ', ';
          }
          $default .= $term->name;
        }
      }

      $form['value'] = array(
        '#title' => t('Select terms from vocabulary @voc', array('@voc' => $vocabulary->name)),
        '#type' => 'textfield',
        '#autocomplete_path' => 'taxonomy/autocomplete/' . $vocabulary->vid,
        '#default_value' => $default,
      );
    }
    else {
      if (!empty($this->options['hierarchy'])) {
        $tree = taxonomy_get_tree($vocabulary->vid);
        $options = array();

        if ($tree) {
          foreach ($tree as $term) {
            $choice = new stdClass();
            $choice->option = array($term->tid => str_repeat('-', $term->depth) . $term->name);
            $options[] = $choice;
          }
        }
      }
      else {
        $options = array();
        $result = db_query("SELECT * FROM {term_data} WHERE vid = %d ORDER BY weight, name", $vocabulary->vid);
        while ($term = db_fetch_object($result)) {
          $options[$term->tid] = $term->name;
        }
      }

      // Since we're overriding, allow the list of options to be reduced.
      if (!empty($form_state['exposed']) && !empty($this->options['expose']['reduce'])) {
        $options = $this->reduce_value_options($options);
      }

      $form['value'] = array(
        '#type' => 'select',
        '#title' => t('Select terms from vocabulary @voc', array('@voc' => $vocabulary->name)),
        '#multiple' => TRUE,
        '#options' => $options,
        '#size' => min(9, count($options)),
        '#default_value' => !empty($this->value) ? $this->value : array(),
      );
    }

    if (empty($form_state['exposed'])) {
      // Retain the helper option
      $this->helper->options_form($form, $form_state);
    }
  }

  function value_validate(&$form, &$form_state) {
    // We only validate if they've chosen the text field style.
    if ($this->options['type'] != 'textfield') {
      return;
    }

    $values = drupal_explode_tags($form_state['values']['options']['value']);
    $tids = $this->validate_term_strings($form['value'], $values);

    if ($tids) {
      $form_state['values']['options']['value'] = $tids;
    }
  }

  function accept_exposed_input($input) {
    $rc = parent::accept_exposed_input($input);

    if ($rc) {
      // If we have previously validated input, override.
      if (isset($this->validated_exposed_input)) {
        $this->value = $this->validated_exposed_input;
      }
    }

    return $rc;
  }

  function exposed_validate(&$form, &$form_state) {
    if (empty($this->options['exposed'])) {
      return;
    }

    $identifier = $this->options['expose']['identifier'];

    // We only validate if they've chosen the text field style.
    if ($this->options['type'] != 'textfield') {
      if (!empty($this->options['expose']['single']) && $form_state['values'][$identifier] != 'All')  {
        $this->validated_exposed_input = array($form_state['values'][$identifier]);
      }
      return;
    }

    if (empty($this->options['expose']['identifier'])) {
      return;
    }

    $values = drupal_explode_tags($form_state['values'][$identifier]);

    $tids = $this->validate_term_strings($form[$identifier], $values);
    if ($tids) {
      $this->validated_exposed_input = $tids;
    }
  }

  /**
   * Validate the user string. Since this can come from either the form
   * or the exposed filter, this is abstracted out a bit so it can
   * handle the multiple input sources.
   */
  function validate_term_strings(&$form, $values) {
    if (empty($values)) {
      return array();
    }

    $tids = array();
    $placeholders = array();
    $args = array();
    $results = array();
    foreach ($values as $value) {
      $missing[strtolower($value)] = TRUE;
      $args[] = $value;
      $placeholders[] = "'%s'";
    }

    if (!$args) {
      return;
    }

    $result = db_query("SELECT * FROM {term_data} WHERE name IN (" . implode(', ', $placeholders) . ")", $args);
    while ($term = db_fetch_object($result)) {
      unset($missing[strtolower($term->name)]);
      $tids[] = $term->tid;
    }

    if ($missing) {
      form_error($form, format_plural(count($missing), 'Unable to find term: @terms', 'Unable to find terms: @terms', array('@terms' => implode(', ', array_keys($missing)))));
    }

    return $tids;
  }

  function value_submit($form, &$form_state) {
    // prevent array_filter from messing up our arrays in parent submit.
  }

  function expose_form_right(&$form, &$form_state) {
    parent::expose_form_right($form, $form_state);
    if ($this->options['type'] != 'select') {
      unset($form['expose']['reduce']);
    }
  }

  function admin_summary() {
    // set up $this->value_options for the parent summary
    $this->value_options = array();

    if ($this->value) {
      $result = db_query("SELECT * FROM {term_data} td WHERE td.tid IN ("  . implode(', ', $this->value) . ")");

      while ($term = db_fetch_object($result)) {
        $this->value_options[$term->tid] = $term->name;
      }
    }
    return parent::admin_summary();
  }
}
