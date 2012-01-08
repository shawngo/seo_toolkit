<?php

/**
 * File SEO Toolkit admin callback file
 */

/**
 * Implementation of hook_admin_settings() for configuring the module
 */
function seo_toolkit_admin_settings_form(&$form_state) {
  // Link specific configurations.
  $form['general_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('General Settings'),
    '#description' => t('Settings go here...'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $form['general_settings']['seo_toolkit_setting'] = array(
    '#type' => 'textfield',
    '#title' => t('No settings like home'),
    '#default_value' => variable_get('seo_toolkit_setting', 'This setting doesn\'t do anything'),
    '#description' => t('Nothing, this setting does.')
  );

  return system_settings_form($form);
}


/**
 * Form callback for title tag CSV import.
 */
function seo_toolkit_form(&$form_state) {
  global $user;
  drupal_set_title('Title tag importer');

  $form = array();
  $form['#attributes'] = array('enctype' => "multipart/form-data");

  $form['import'] = array(
    '#type' => 'fieldset',
    '#title' => t('Import settings'),
    '#description' => t('Adhere to all the rules.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['import']['description'] = array(
    '#type' => 'textfield',
    '#title' => t('Description of import'),
    '#description' => t('Used to identify each action'),
    '#default_value' => 'Importing some titles - ' . $user->name,
    '#required' => TRUE,
  );

  $form['file_import'] = array(
    '#type' => 'fieldset',
    '#title' => t('File upload'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['file_import']['instructions'] = array(
    '#type' => 'markup',
    '#title' => t('Instructions'),
    '#value' => '<p class="description">Import format is a comma-separated .csv file formatted as follows:
        <br /><code>nid,page_title<br />"123","My SEO Title"</code></p>',
  );
  $form['file_import']['source'] = array(
    '#type' => 'file',
    '#title' => t('File'),
    '#description' => t("Upload CSV file"),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Import'),
  );
  $form['submit_test'] = array(
    '#type' => 'submit',
    '#value' => t('Test import'),
  );

  return $form;
}

/**
 * Validate callback for imports.
 */
function seo_toolkit_form_validate($form, &$form_state) {
  // if error
  // form_set_error('redirect', t('You are attempting to redirect the page to itself. This will result in an infinite loop.'));
  if (empty($_FILES['files']['tmp_name']['source'])) {
    form_set_error('source', t('File is required.'));
  }
}

/**
 * Submit handler for imports.
 */
function seo_toolkit_form_submit($form, &$form_state) {
  global $user;

  // process the file
  $file = $_FILES['files']['tmp_name']['source'];

  // Not implementing this for now.
  if ($save_imports = FALSE) {
    $filename = 'seo_toolkit_import.csv';
    $dest = 'seo_toolkit/import-'. time() .'-'. $filename;
    file_save_upload($file, array(), file_directory_path() .'/'. $dest, FILE_EXISTS_REPLACE);
  //  file_save_data($report, $file, FILE_EXISTS_REPLACE);
  }
  $process = FALSE;
  switch ($form_state['clicked_button']['#value']) {
    case t('Import'):
      $process = TRUE;
      break;
    case t('Test import'):
      $process = FALSE;
      break;
  }
  $output = seo_toolkit_process_upload($file, $process);
  drupal_set_message($output);
  seo_toolkit_log('import', $user->uid, check_plain($form_state['values']['description']));
}


/**
 * Do the actual file processing
 */
function seo_toolkit_process_upload($file, $process = TRUE) {

  if (($handle = fopen($file, "r")) !== FALSE) {
    $row = 0;
    $from = 0;
    $to = 1;
    while (($data = fgetcsv($handle, 1000, ',')) !== FALSE) {
      if ($row == 0) {
       $header = $data;

      }
      else {
        $output[] = $data;
      }

      // prep for import
      if ($process) {

      }
      // prep for display
      else {

      }
      $row++;
    }
    fclose($handle);
  }

  // process data
  if ($process) {
    $msg = seo_toolkit_process_import($output);
    drupal_set_message($msg['error'], $msg['status']);
  }
  // display as table
  else {
    drupal_add_js('misc/collapse.js');
    return theme('fieldset', array(
      '#title' => t('Results'),
//      '#value' => theme('table', $header, $output),
      '#value' => seo_toolkit_process_import_test($output),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    ));
  }

}

/**
 * function to process an array of url aliases
 */
function seo_toolkit_process_import_test($imports) {
  if (empty($imports)) {
    return t('Import file is empty');
  }
  $dups = array();
  $status = array(
    'updates' => 0,
    'imports' => 0,
    'missing' => array(),
  );

  $update = array();
  foreach ($imports as $index => $import) {
    $node = node_load($import[0], NULL, TRUE);
    $title = trim($import[1]);
    // Check for duplicate import entries.
    $dups[$node->nid]++;
    $action = '';
    $title = trim($import[1]);
    // Node doesn't exist or title is blank.
    if (!$node) {
      // @TODO add error/notification.
      $status['missing'][] = $import;
      $action = 'missing';
    }
    else if ($title == '') {
      $status['deletes']++;
      $action = 'delete';
    }
    // If the node has a title.
    else if (page_title_load_title($node->nid, 'node')) {
      $update = array('id');
      $status['updates']++;
      $action = 'update';
    }
    // If this is an insert.
    else {
      $status['imports']++;
      $action = 'import';
    }


    $page_title['nid'] = $action == 'missing' ? $import[0] : $node->nid;
    $page_title['type'] = 'node';

    if ($title != '') {
      try {
        $page_title['page_title'] = check_plain($title);
      }
      catch (Exception $e) {
        drupal_set_message('Error on node ' . $import[0] . ' row ' . $index);
        continue;
      }
    }

    $page_title['action'] = $action;
    $page_title['duplicate'] = $dups[$node->nid] > 1 ? 'Yes' : 'No';

    $rows[] = $page_title;
  }

  $dup_count = 0;
  foreach ($dups as $nid => $count) {
    if ($count > 1) {
      $dup_count++;
      $duplicates .= 'Node '. $nid .' has ' . $count . ' import entries <br />';
    }
  }

  foreach ($status['missing'] as $missing) {
    $missing_items .= 'Node ' . $missing[0] . ' does not exist<br />';
  }
  $message = '<p>Items imported: ' . $status['imports'] . '<br />';
  $message .= 'Items updated: ' . $status['updates'] . '<br />';
  $message .= 'Items deleted: ' . $status['deletes'] . '<br />';
  $message .= 'Items missing: ' . count($status['missing']) . '<br />';
  $message .= 'Duplicate items: ' . $dup_count . '</p>';
  $message .= '<p>' . $duplicates . $missing_items . '</p>';
  $message .= theme('table', array_keys($rows[0]), $rows);

  return $message;
}

/**
 * function to process an array of url aliases
 */
function seo_toolkit_process_import($imports) {
  if (empty($imports)) {
    return t('Import file is empty');
  }
  $dups = array();
  $status = array(
    'updates' => 0,
    'deletes' => 0,
    'imports' => 0,
    'missing' => array(),
  );

  foreach ($imports as $index => $import) {
    $node = node_load($import[0], NULL, TRUE);
    $title = trim($import[1]);

    if (!$node) {
      // @TODO add error/notification.
      $status['missing'][] = $import;
      continue;
    }

    // Check for duplicate import entries.
    $dups[$node->nid]++;

    if (page_title_load_title($node->nid, 'node') != '') {
      $update = array('id');
      $status['updates']++;
    }
    else {
      $update = array();
      $status['imports']++;
    }
    $page_title = new stdClass();
    $page_title->id = $node->nid;
    $page_title->type = 'node';

    if ($title != '') {
      try {
        $page_title->page_title = check_plain($title);
      } catch (Exception $e) {
        drupal_set_message('Error on node ' . $import[0] . ' row ' . $index);
      }
    }
    else {
      $status['deletes']++;
    }
    // @TODO use page_title function to respect any hooks.
    if (!drupal_write_record('page_title', $page_title, $update)) {
      $status[empty($update) ? 'imports' : 'updates']--;
      drupal_set_message('Could not import ' . print_r($page_title, TRUE));
    }
  }

  $dup_count = 0;
  foreach ($dups as $nid => $count) {
    if ($count > 1) {
      $dup_count++;
      $duplicates .= 'Node '. $nid .' has ' . $count . ' import entries<br />';
    }
  }

  foreach ($status['missing'] as $missing) {
    $missing_items .= 'Node ' . $missing[0] . ' does not exist<br />';
  }
  $message = 'Items inserted: ' . $status['imports'] .'<br />';
  $message .= 'Items updated: ' . $status['updates'] .'<br />';
  $message .= 'Items deleted: ' . $status['deletes'] .'<br />';
  $message .= 'Items missing: ' . count($status['missing']) .'<br />';
  $message .= 'Duplicate items: ' . $dup_count .'<br />';
  drupal_set_message($message . $duplicates . $missing_items);
//  page_title_load_title($node->nid, 'node')
}


function seo_toolkit_export_form(&$form_state) {
  $form['export'] = array(
    '#type' => 'fieldset',
    '#title' => t('Export settings'),
    '#description' => t('idk yet.'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['export']['description'] = array(
    '#type' => 'textfield',
    '#title' => t('Description of export'),
    '#description' => t('Activity entry'),
    '#required' => TRUE,
  );
  $form['export']['export_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Export'),
  );

  return $form;
}


/**
 * Export callback.
 */
function seo_toolkit_export_form_submit($form, &$form_state) {


  $filename = 'seo_toolkit_export.csv';
  drupal_set_header('Content-Type: text/csv');
  drupal_set_header('Content-Disposition: attachment; filename=' . $filename);

  $sql = 'SELECT id, page_title from {page_title} WHERE type = "node"';

  $result = db_query($sql);
  $count = mysql_num_fields($result);
  for($i = 0; $i < $count; $i++){
    $header[] = mysql_field_name($result, $i);
  }

  $report = implode(',', $header) ."\r\n";

  $i = 0;

  while($row = db_fetch_array($result)){
    $i++;
    foreach($row as $value){
      $values[] = '"' . str_replace('"', '""', decode_entities($value)) . '"';
    }
    $report .= implode(',', $values) ."\r\n";
    unset($values);
  }

  echo $report;
  exit;
}

function seo_toolkit_log($action, $uid, $description) {
  $seo_toolkit = new stdClass();

  $seo_toolkit->action = $action;
  $seo_toolkit->uid = $uid;
  $seo_toolkit->description = $description;
  $seo_toolkit->timestamp = time();

  return drupal_write_record('seo_toolkit_log', $seo_toolkit);
}


function seo_toolkit_options() {
  $options = array(0 => t('None'));
  $result = db_query('SELECT * FROM {seo_toolkit} ORDER BY rid ASC');
  while ($row = db_fetch_array($result)) {
    $options[$row['rid']] = strtoupper($row['rollback_type']) .': '. check_plain($row['description']) .' <em>('. date("F j, Y, g:i a", $row['timestamp']) .')</em>';
  }

  return $options;
}

function array_to_CSV($data) {
  $outstream = fopen("php://temp", 'r+');
  fputcsv($outstream, $data, ',', '"');
  rewind($outstream);
  $csv = fgets($outstream);
  fclose($outstream);
  return $csv;
}

function CSV_to_array($data) {
  $instream = fopen("php://temp", 'r+');
  fwrite($instream, $data);
  rewind($instream);
  $csv = fgetcsv($instream, 9999999, ',', '"');
  fclose($instream);
  return($csv);
}
