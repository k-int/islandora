<?php

/**
 * @file datastream.inc 
 */

define('DS_COMP_STREAM', 'DS-COMPOSITE-MODEL');
/**
 *
 * @global object $user
 * @param string $object_id
 * @param string $dsid
 * @return stream 
 *  prints datastream to browser
 */
function islandora_datastream_as_attachment($object_id, $dsid) {
  module_load_include('inc', 'islandora', 'RestConnection');
  global $user;
  try {
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($object_id, $restConnection->repository);

    // if the object exists but the datastream doesn't
    if(!isset($fedora_object[$dsid])) {
      return drupal_not_found();
    }

    header('Content-type: ' . $fedora_object[$dsid]->mimetype);
    if($fedora_object[$dsid]->controlGroup == 'M' || $fedora_object[$dsid]->controlGroup == 'X') {
      header('Content-length: ' . $fedora_object[$dsid]->size);
    }
    header("Cache-control: private");
    $method = arg(5);
    if (isset($method) && $method == 'download') {
      header("Content-Disposition: attachment; filename=\"" . $fedora_object[$dsid]->label);
    }
    print($fedora_object[$dsid]->content);
    exit();
  } catch (Exception $e) {
    return drupal_not_found();
  }

}

function islandora_get_datastream_parents($islandora_object) {
  $parent_collections = array();
  $repository = $islandora_object->repository;
  $collections1 = $islandora_object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection');
  $collections2 = $islandora_object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');
  $collections = array_merge($collections1, $collections2);

  foreach($collections as $collection) {
    try {
      $pid = $collection['object']['value'];
      $object = $repository->getObject($collection['object']['value']);
      $parent_collections[$pid] = array();
      $parent_collections[$pid]['object'] = $object;
      $parent_collections[$pid]['url'] = 'islandora/object/' . $object->id;
      $parent_collections[$pid]['label'] = $object->label;
      $parent_collections[$pid]['label_link'] = l($parent_collections[$pid]['label'], $parent_collections[$pid]['url']);
    }
    catch (RepositoryException $e) {}
  }

  return $parent_collections;
}

/**
 *
 * @param array $arr
 *   an array of dsids that are defined by this objects cmodels
 * @param string $ds_comp_stream 
 *   the dscomposite stream as xml
 */
function islandora_get_defined_dsids_array(&$arr, $ds_comp_stream) {
  $sxml = new SimpleXMLElement($ds_comp_stream);
  foreach ($sxml->dsTypeModel as $ds) {
    //$arr[$ds['ID']] 
    $mimes = array();
    foreach ($ds->form as $form) {
      $mimetype = (string) $form['MIME'];
      $mimes[] = $mimetype;
    }
    $dsid = (string) $ds['ID'];
    if ($dsid != 'AUDIT') {
      $arr[(string) $ds['ID']] = $mimes;
    }
  }
}

/**
 *
 * @global type $user
 * @param string $object_id
 * @return string|array 
 */
function islandora_get_unused_dsids($object_id) {
  module_load_include('inc', 'islandora', 'RestConnection');
  global $user;
  try {
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($object_id, $restConnection->repository);
  } catch (Exception $e) {
    drupal_set_message(t('Error getting Islandora object %s ', array('%s' => $object_id)), 'error');
    return;
  }
  if (!isset($fedora_object)) {
    drupal_set_message(t('Could not create add datastream form for %s'), array('%s' => $object_id));
    return;
  }
  $models = $fedora_object->models;
  $defined_dsids = array();
  if (isset($models)) {
    foreach ($models as $model) {
      try {
        $model_object = new FedoraObject($model, $restConnection->repository);
        if (isset($model_object[DS_COMP_STREAM])) {
          $dscomposite_stream = $model_object[DS_COMP_STREAM]->content;
          islandora_get_defined_dsids_array($defined_dsids, $dscomposite_stream);
        }
      } catch (Exception $e) {
        //do nothing as other objects may have a dscompsite stream
      }
    }
  }
  foreach ($defined_dsids as $key => $value) {
    if (isset($fedora_object[$key])) {
      unset($defined_dsids[$key]); //ds exists in the object so don't show in the dropdown
    }
  }
  return $defined_dsids;
}

/**
 * buids the default add datastream form
 * @param string $object_id
 * @param array $form_state
 * @return array 
 *  a form ready to be rendered with a call to Drupal render
 */
function islandora_get_add_datastream_form($object_id, &$form_state) {
  $unused_dsids = islandora_get_unused_dsids($object_id); //$defined_dsids;
  $form = array();
  $form['add_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => 'Add a datastream',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['add_fieldset']['add_datastream_label'] = array(
    '#value' => t('<br /><h3>Add Datastream:</h3>'),
    '#weight' => -10,
  );

  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => "$object_id"
  );

  $form['add_fieldset']['stream_label'] = array(
    '#title' => 'Datastream Label',
    '#required' => 'TRUE',
    '#description' => t('A Human readable label'),
    '#type' => 'textfield'
  );

  $form['#attributes']['enctype'] = 'multipart/form-data';
  $form['add_fieldset']['add-stream-file-location'] = array(
    '#type' => 'file',
    '#title' => t('Upload Document'),
    '#size' => 48,
    // '#required'=>'TRUE',
    '#description' => t('The file to upload.')
  );
  $form['#redirect'] = "islandora/object/$object_id/";
  $form['add_fieldset']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add Datastream')
  );

  if (!empty($unused_dsids)) {
    $dsidsForForm = array();
    foreach ($unused_dsids as $key => $value) {
      $dsidsForForm[$key] = $key;
    }
    $form['add_fieldset']['stream_id'] = array(
      '#type' => 'select',
      '#title' => t('Datastream ID'),
      '#default_value' => variable_get('feed_item_length', 'teaser'),
      '#weight' => '-1',
      '#description' => t('Datastream IDs defined by the content model.'),
    );
    $form['add_fieldset']['stream_id']['#options'] = $dsidsForForm;
  }
  else {
    $form['add_fieldset']['stream_id'] = array(
      '#title' => 'Datastream ID',
      '#required' => 'TRUE',
      '#description' => t('An ID for this stream that is unique to this object. Must start with a letter and contain only alphanumeric characters and dashes and underscores.'),
      '#type' => 'textfield',
      '#weight' => -1,
    );
  }
  return $form;
}

/**
 * Default implmentation currently only does M (managed datastreams)
 * other modules can hook form alter to add other functionality
 * @global string $base_url
 * @global object $user
 *   Drupal user
 * @param array $form
 * @param array $form_state
 * @return type 
 */
function islandora_add_datastream_form_submit($form, &$form_state) {
  global $base_url;
  module_load_include('inc', 'islandora', 'RestConnection');
  if (!empty($form_state['submit']) && $form_state['submit'] == 'OK') {
    $form_state['rebuild'] = TRUE;
    return;
  }

  module_load_include('inc', 'islandora', 'includes/MimeClass');
  $mimetype = new MimeClass();
  
  $file = $form_state['values']['add-stream-file-location'];
  $file = drupal_realpath($file);

  $object_id = $form_state['values']['pid'];
  $dsid = $form_state['values']['stream_id'];
  $ds_label = $form_state['values']['stream_label'];
  $dformat = $mimetype->getMimeType($file);
  $controlGroup = "M";

  try {
    $restConnection = new RestConnection();
    $fedora_object = $restConnection->repository->getObject($object_id);
    $ds = $fedora_object->constructDatastream($dsid, $controlGroup);
    $ds->label = $ds_label;
    $ds->mimetype = $dformat;
    $ds->setContentFromFile($file);
    $fedora_object->ingestDatastream($ds);
    $d_file = file_load($form_state['values']['fid']);
    file_delete($d_file);
  } catch (exception $e) {
    drupal_set_message(t('@message', array('@message' => check_plain($e->getMessage()))), 'error');
    return;
  }
  drupal_set_message("Successfully Added Datastream!");
  drupal_goto("islandora/object/$object_id");
}

/**
 * validates this datastream id against its allowed mimetypes in the dscomposite 
 * of its content models.
 * @param array $form
 * @param array $form_state
 * @return boolean 
 */
function islandora_add_datastream_form_validate($form, &$form_state) {
  module_load_include('inc', 'islandora', 'includes/MimeClass');
  module_load_include('inc', 'islandora', 'RestConnection');
  $mimetype = new MimeClass();
  if ($form_state['clicked_button']['#value'] == 'OK') {
    $form_state['rebuild'] = TRUE;
    return;
  }
  $dsid = $form_state['values']['stream_id'];
  $dsLabel = $form_state['values']['stream_label'];
  if (strlen($dsid) > 64) {
    form_set_error('', t('Data stream ID cannot be more than 64 characters.'));
    return FALSE;
  }
  if (!(preg_match("/^[a-zA-Z]/", $dsid))) {
    form_set_error('', t("Data stream ID (@dsid) has to start with a letter.", array('@dsid' => check_plain($dsid))));
    return FALSE;
  }
  if (strlen($dsLabel) > 64) {
    form_set_error('', t('Data stream Label cannot be more than 64 characters.'));
    return FALSE;
  }
  if (strpos($dsLabel, '/')) {
    form_set_error('', t('Data stream Label cannot contain a "/".'));
    return FALSE;
  }

  $object_id = $form_state['values']['pid'];
  $restConnection = new RestConnection();
  $fedora_object = $restConnection->repository->getObject($object_id);

  if(isset($fedora_object[$dsid])) {
    form_set_error('', t('Data stream ID already exists in object.'));
    return FALSE;
  }

  $mimetype = new MimeClass();
  $unused_dsids = islandora_get_unused_dsids($form_state['values']['pid']);
  if(isset($unused_dsids[$dsid])) {
    $types_allowed = $unused_dsids[$dsid];
    $arr = array();
    foreach ($types_allowed as $type) {
      $arr[] = $mimetype->getExtension($type);
    }
  }
  else {
    // todo: this is unsafe, should probably be fixed see:
    // http://api.drupal.org/api/drupal/includes!file.inc/function/file_save_upload/7
    $arr = array();
  }
  

  $file = file_save_upload('add-stream-file-location', array('file_validate_extensions' => $arr));
  if ($file) {
    $form_state['values']['add-stream-file-location'] = $file->uri;
    $form_state['values']['fid'] = $file->fid; //so we can load it to delete later
  }
  else {
    form_set_error('add-stream-file-location', t('There was no file uploaded'));
  }
}


/**
 * buids the default add datastream form
 * @param string $object_id
 * @param array $form_state
 * @return array 
 *  a form ready to be rendered with a call to Drupal render
 */
function islandora_add_datastream_form($form, &$form_state, $object_id) {
  $unused_dsids = islandora_get_unused_dsids($object_id); //$defined_dsids;
  $form = array();
  $form['add_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => 'Add a datastream',
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['add_fieldset']['add_datastream_label'] = array(
    '#value' => t('<br /><h3>Add Datastream:</h3>'),
    '#weight' => -10,
  );

  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => "$object_id"
  );

  $form['add_fieldset']['stream_label'] = array(
    '#title' => 'Datastream Label',
    '#required' => 'TRUE',
    '#description' => t('A Human readable label'),
    '#type' => 'textfield'
  );

  $form['#attributes']['enctype'] = 'multipart/form-data';
  $form['add_fieldset']['add-stream-file-location'] = array(
    '#type' => 'file',
    '#title' => t('Upload Document'),
    '#size' => 48,
    // '#required'=>'TRUE',
    '#description' => t('The file to upload.')
  );
  $form['#redirect'] = "islandora/object/$object_id/";
  $form['add_fieldset']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add Datastream')
  );

  $unused_dsids = islandora_get_unused_dsids($object_id);
  $dsidsForForm = '';
  $i = 0;
  foreach ($unused_dsids as $key => $value) {
    if($i++) {
      $dsidsForForm .= ", ";
    }
    $dsidsForForm .= "'$key'";
  }

  $form['add_fieldset']['stream_id'] = array(
    '#title' => 'Datastream ID',
    '#required' => 'TRUE',
    '#description' => t('An ID for this stream that is unique to this object. Must start with a letter and contain only alphanumeric characters and dashes and underscores. Datastreams that are defined by the content model dont currently exist: <b>' . $dsidsForForm . '</b>.'),
    '#type' => 'textfield',
    '#weight' => -1,
    '#autocomplete_path' => "islandora/object/$object_id/manage/datastreams/add/autocomplete",
  );
  return $form;
}

function islandora_datastream_autocomplete_callback($object_id, $string = '') {
  $dsids = islandora_get_unused_dsids($object_id);
  $output = array();
  foreach($dsids as $id => $ds) {
    if(trim($string) == '') {
      $output[$id] = $id;
    }
    else {
      $ret = stripos($id, $string);
      if($ret !== FALSE) {
        $output[$id] = $id;
      }
    }
  }
  drupal_json_output($output);
}

function islandora_datastream_get_human_readable_size($ds) {
  module_load_include('inc', 'islandora', 'includes/utilities');

  // we return - if we don't have a size
  if($ds->controlGroup == 'M' || $ds->controlGroup == 'X') {
    return islandora_convert_bytes_to_human_readable($ds->size);
  }
  else {
    return '-';
  }
}

function islandora_datastream_get_url($ds, $type = 'download') {
  if($ds->controlGroup == 'R') {
    return $ds->url;
  }
  else {
    return "islandora/object/{$ds->parent->id}/datastream/{$ds->id}/$type";
  }
}

function islandora_datastream_edit_get_link($object, $ds_id) {
  $edit_registry = module_invoke_all('islandora_edit_datastream_registry', $object, $ds_id);
  if (count($edit_registry) > 0 && user_access(FEDORA_METADATA_EDIT)) {
    return l(t('edit'), 'islandora/object/' . $object->id . '/datastream/' . $ds_id . '/edit');
  }
  else {
    return '';
  }
}

function islandora_edit_datastream($object_id, $ds_id) {
  global $user;
  try {
    module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $object = new FedoraObject($object_id, $restConnection->repository);
    
    $edit_registry = module_invoke_all('islandora_edit_datastream_registry', $object, $ds_id);
    $edit_count = count($edit_registry);
  
    if ($edit_count == 0) {
      // No edit implementations.
      drupal_set_message(t('There are no edit methods specified for this datastream.'));
      drupal_goto('islandora/object/' . $object->id . '/manage/datastreams');
    }
    elseif ($edit_count == 1) {
      // One registry implementation, go there
      drupal_goto($edit_registry[0]['url']);
    }
    else {
      // Multiple edit routes registered
      return islandora_edit_datastream_registry_render($edit_registry);
    }
  } catch (Exception $e) {
    drupal_set_message(t('Error getting Islandora edit method for %s ', array('%s' => $object_id)), 'error');
    return;
  }
  
}

//@TODO: theme / preprocess
function islandora_edit_datastream_registry_render($edit_registry) {
  $output = array(
    '#type' => 'markup',
    '#markup' => '',
  );
  foreach ($edit_registry AS $edit_route) {
    $output['#markup'] .= l($edit_route['name'], $edit_route['url']) . '<br/>';
  }
  return $output;
}