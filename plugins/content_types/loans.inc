<?php

/**
 * Plugins are described by creating a $plugin array which will be used
 * by the system that includes this file.
 */
$plugin = array(
  'title' => t('User loan list'),
  'description' => t('The current loans for a user'),
  'single' => TRUE,
  'content_types' => array('list'),
  'required context' => new ctools_context_required(t('User'), 'user'),
  'category' => t('User'),
);

/**
 * Render the block
 */
function ding_loan_loans_content_type_render($subtype, $conf, $panel_args, $context) {
  $account = isset($context->data) ? $context->data : NULL;
  $items = array();
  $ids = array();

  $block = new stdClass();
  $block->module = 'ding_loan';
  $block->delta = 'loans';
  $block->title = t('Loan list');

  $list = ding_provider_invoke_page('loan', 'list', $account);
  foreach ($list as $item) {
    $ids[] = $item->ding_entity_id;
  }
  // Preload all entities.
  ding_entity_load_multiple($ids);

  $block->content = ding_provider_get_form('ding_loan_loans_form', $account, $list);
  return $block;
}

/**
 * Adding the admin form, to be able to control the required context
 */
function ding_loan_loans_content_type_edit_form($form, &$form_state) {
  return $form;
}

/**
 * Implements a drupal form, which show a list of reservations
 */
function ding_loan_loans_form($form, &$form_state, $account, $items = array()) {
  //Added datepicker to be used for overdue calculation 
  drupal_add_library('system','ui.datepicker');
  $options = array();
  $destination = drupal_get_destination();
  $header = array(
    'title' => array('data' => t('Title')),
    'notes' => t('periodical'),
    'loan_date' => array('data' => t('Loan date'),'class' => '{sorter: "ting_date_sort"}'),
    'expiry' => array('data' => t('Return date'),'class' => '{sorter: "ting_date_sort"}'),
    'operations' => array('data' => '', 'class' => '{sorter: false}'),
  );
  uasort($items, 'ding_loan_sort_expiry');

  $form['account'] = array(
    '#type' => 'value',
    '#value' => $account,
  );

  $form['items'] = array(
    '#type' => 'value',
    '#value' => $items,
  );

  foreach ($items as $id => $item) {
    $item->creator = 'fake';
    $entity = $item->entity;
      $res = NULL;
      $ting_title = $entity != false ? $entity->title : null;
      
      if (isset($ting_title) && $ting_title == FALSE) {
      db_delete('ting_object')
          ->condition('tid', $entity->tid)
          ->execute();
      $entity = array();
    }
    //Check if remote loan then get title.
        if ($entity == FALSE) {
          $ding_entity_id = explode(":", $item->ding_entity_id);
          $result = ding_provider_invoke('availability', 'holdings', array($ding_entity_id[1]));
          $details = $result[$ding_entity_id[1]];      
          if (isset($details['title'])) {
          $title = array('#markup' =>$details['title']);
          }
        }
         elseif ($item->entity->type == 'Tidsskrift') {
          global $user;
          $notes = ding_provider_invoke('loan', 'get_loans', $user);
          $res   = $notes[$item->id];
          $title = array('#markup' => $item->display_name);
        }
        else {
          $title = array('#markup' => $item->display_name);
        }    
    $options[$item->id] = array(
      '#disabled' => $item->renewable,
      'title' => array(
        'data' => array(
          $entity ? ting_object_view($entity, 'user_list') : $title,
        ),
        'class' => 'title',
        ),
       'notes' => array(
          'data' => $res['notes'] ? check_plain($res['notes']) : '',
          'class' => 'notes',
        ),
      'loan_date' => array(
          'data' => format_date(strtotime(check_plain($item->loan_date)), 'date_only'),
          'class' => 'created-date',
        ),
      'expiry' => array(
          'data' => format_date(strtotime(check_plain($item->expiry)), 'date_only'),
          'class' => 'expire-date',
        ),
      'operations' => array(
        'data' =>  array(
        ),
        'class' => 'operations',
      ),
    );

    if ($item->materials_number) {
      $options[$item->id]['title']['data'][] = array(
        '#type' => 'markup',
        '#prefix' => '<p class="order-id">',
        '#markup' => t('Material no. @mat_no', array('@mat_no' => $item->materials_number)),
        '#suffix' => '</p>',
      );
    }
    if ($item->renewable) {
      $options[$item->id]['operations']['data'][] = array(
          '#type' => 'submit',
          '#prefix' => '<div class="renew-loan-button">',
          '#value' => t('Renew'),
          '#submit' => array('ding_loan_loans_form_renew_submit'),
          '#loan_id' => $item->id,
          '#name' => 'renew-' . $item->id, // Needed so form API can tell them
                                           // apart.
          '#disabled' => !$item->renewable,
          '#suffix' => '</div>',
      );
    }
      elseif (!$item->renewable) {
      $options[$item->id]['title']['data'][] = array(
        '#type' => 'markup',
        '#prefix' => '<p class="not-renewable">',
        '#markup' => t('Not renewable'),
        '#suffix' => '</p>',
      );
    }
  }
    
  $form['loans'] = array(
    '#type' => 'tableselect_form',
    '#header' => $header,
    '#options' => $options,
    '#empty' => t('No loans'),
    '#process' => array('ding_base_element_process_tableselect_form', '_ding_loan_loans_disable'),
  );
  /*
   * Add css
   */
  drupal_add_css(drupal_get_path('theme', 'latto_odense') .'/css/oc_tablesort.css');
  $form['loans']['#attributes']['class'] = array('tablesorter','{sortlist: [[4,0]]}');
  /*
   * Add JS
   */
   $form['#attached']['js'] = array(
    drupal_get_path('module', 'ding_loan') . '/js/ding_loan.js',
  );
  
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#prefix' => '<div class="renew-loan-button">',
    '#value' => t('Renew selected'),
    '#suffix' => '</div>',
  );

  return $form;
}
        

/**
 * Process function to disable checkboxes for unrenewable materials.
 */
function _ding_loan_loans_disable($element) {
  foreach ($element['#options'] as $key => $choice) {
    if (isset($choice['#disabled']) && !$choice['#disabled']) {
      $element[$key]['#disabled'] = TRUE;
      $element[$key]['#attributes']['title'] = t('Not renewable');      
    }
  }
  return $element;
}

/**
 * Submit handler for the form.
 */
function ding_loan_loans_form_submit($form, &$form_state) {
  $ids = array();
  foreach (array_filter($form_state['values']['loans']) as $id) {
    // The loans array has _form_options array, henc the is_string check.
    if (is_string($id) && $form_state['values']['items'][$id]->renewable) {
      $ids[] = $id;
    }
  }

  $result = ding_provider_invoke('loan', 'renew', $form_state['values']['account'], $ids);
  foreach ($result as $id => $status) {
    if ($object = $form_state['values']['items'][$id]->entity) {
      $title = $object->title;
    }
    elseif ($object == false){
    $title = $form['loans']['#options'][$id]['title']['data'][0]['#markup'];

    }
    else {
      $title = $item->display_name;
    }
    // @todo Better feedback.
    switch ($status) {
      case DingProviderLoan::STATUS_RENEWED:
        drupal_set_message(t('"@title renewed"', array('@title' => $title)));
        break;
      case DingProviderLoan::STATUS_NOT_RENEWED:
        drupal_set_message(t('"@title not renewed"', array('@title' => $title)), 'warning');
        break;
      case DingProviderLoan::STATUS_INT_LOAN:
          drupal_set_message(t('"@title renewed , but remote library can recall the material before time."', array('@title' => $title)));
        break;
      default:
        watchdog('ding_loan', 'Unknown renewal status code @code.', array('@code' => $status), WATCHDOG_ERROR);
    }
  }
}

/**
 *
 */
function ding_loan_loans_form_renew_submit($form, &$form_state) {
  form_set_value($form['loans'], array($form_state['triggering_element']['#loan_id']), $form_state);
  ding_loan_loans_form_submit($form, $form_state);
}

