<?php

/**
 * @file
 *
 * Web UI for configuration of migration from PHPBB to Drupal.
 */

/**
 * Menu callback: Returns a page for configuring phpbb migrations.
 */
function phpbb2drupal_configure() {
  drupal_set_title(t('Phpbb migrate configuration'));
  return drupal_get_form('phpbb2drupal_configure_form');
}

/**
 * Form for configuring phpbb migrations.
 */
function phpbb2drupal_configure_form($form, &$form_state) {
 //TODO: show languages already set up. Allow selection editing and deletion.
 
  $form['overview'] = array(
    '#prefix' => '<div>',
    '#markup' => t('The import of phpbb forums into Drupal can be configured using
      this form.<br />
      Once the configuration has been saved here, the various migrations will appear
      in the Migrate tab.<br />
      You may either provide the necessary credentials for Drupal to access
      your phpbb database and files directly, or the data must be in the current
      Drupal database and all files should have been copied into public storage.'),
    '#suffix' => '</div>',
  );
  
  $form['phpbb2drupal_import_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#required' => TRUE,
    '#description' => t('It is possible to import from multiple PHPBB sources, enter a name here to identify this patuclar source')
    );
  
  if(drupal_multilingual()) {
   $languages = language_list('enabled');
   $languages = $languages[1];
   $names = array('UND' => 'undefined');
   foreach ($languages as $langcode => $item) {
    $name = t($item->name);
    $names[$langcode] = $name . ($item->native != $name ? ' (' . $item->native . ')' : '');
   }
   
   $form['phpbb2drupal_language'] = array(
     '#type' => 'select',
     '#title' => t('Language'),
     '#required' => TRUE,
     '#options' =>  $names,
     '#description' => t('Enter the language of the phpBB content here, if it is mixed or unxnkown use \'UND\'')
   );
  }
  else {
   $form['language_note'] = array(
     '#prefix' => '<div>',
     '#markup' => t('To Import in multple languages you need to turn on the  \'locale\' module and add languages'),
     '#suffix' => '</div>',
   );
  }
  // Select default text format for bodies etc.
  $options = array();
  foreach (filter_formats() as $format_id => $format) {
    $options[$format_id] = $format->name;
  }
  $form['phpbb2drupal_text_format'] = array(
    '#type' => 'select',
    '#title' => t('Format for text fields'),
    '#default_value' => variable_get('phpbb2drupal_text_format', 'filtered_html'),
    '#options' => $options,
    '#description' => t('Which input format to use for all content'),
  );

/*
  // Might need this bit for import of attachments etc
  if (module_exists('media') && !module_exists('migrate_extras')) {
    $form['need_extras'] = array(
      '#prefix' => '<div>',
      '#markup' => t('You have the <a href="@media">Media module</a> enabled - to
        take advantage of Media features, you need to also install and enable the
        <a href="@extras">Migrate Extras module</a>.',
        array('@media' => url('http://drupal.org/project/media'),
          '@extras' => url('http://drupal.org/project/migrate_extras'))),
      '#suffix' => '</div>',
    );
  }
 */
  $form['files'] = array(
    '#type' => 'fieldset',
    '#title' => t('Phpbb files location'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['files']['description'] = array(
    '#prefix' => '<div>',
    '#markup' => t('To import your forum files (avatars, smilies, attachments,
                    etc) from phpbb, enter the location here. You can point the
                    migration at the location of the website directly, or pick one
                    of the directories in the Drupal public file system. The
                    migration will look for the relevant directories, as determined
                    from the phpbb configuration, under this directory, only if you
                    have not set a domain.'),
    '#suffix' => '</div>',
  );

  $form['files']['phpbb2drupal_files_domain'] = array(
    '#type' => 'textfield',
    '#title' => t('Domain of your forum'),
    '#default_value' => variable_get('phpbb2drupal_files_domain'),
    '#description' => t('Enter the domain of the forum to import, with path if
                         necessary (e.g., http://example.phpbb.com/forum).'),
  );

  $dirs = array('' => '<none>');
  $path = drupal_realpath('public://');
  foreach (scandir($path) as $dir) {
    $dir_path = drupal_realpath('public://' . $dir);
    if (drupal_substr($dir, 0, 1) != '.' && is_dir($dir_path)) {
      $dirs[$dir] = $dir;
    }
  }
  $form['files']['phpbb2drupal_files_directory'] = array(
    '#type' => 'select',
    '#title' => t('Directory of phpbb files'),
    '#default_value' => variable_get('phpbb2drupal_files_directory', ''),
    '#options' => $dirs,
    '#description' => t('Enter the directory name of the forum files in local storage.'),
  );

  $form['db'] = array(
    '#type' => 'fieldset',
    '#title' => t('phpbb database location'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  global $databases;

  $form['db']['phpbb2drupal_db_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('PHPBB Table prefix'),
    '#default_value' => variable_get('phpbb2drupal_db_prefix', 'phpbb_'),
    '#description' => t('The prefix on all table names in the phpbb database'),
  );

  $form['db']['phpbb2drupal_db_local'] = array(
    '#type' => 'checkbox',
    '#title' => t('Use local database'),
    '#default_value' => variable_get('phpbb2drupal_db_local', 1),
    '#description' => t('The PHPBB data has been imported into the Drupal database, ignore credentials below'),
  );

  $form['db']['phpbb2drupal_db_driver'] = array(
    '#type' => 'select',
    '#title' => t('DB Driver'),
    '#default_value' => db_driver(),
    '#options' => array('mysql' => 'MySQL', 'pgsql' => 'PostgreSQL', 'sqlite' => 'SQL Lite'),
    '#description' => t('The type of database server to connect to'),
  );

  $form['db']['phpbb2drupal_db_host'] = array(
    '#type' => 'textfield',
    '#title' => t('DB Hostname'),
    '#default_value' => variable_get('phpbb2drupal_db_host', $databases['default']['default']['host']),
    '#description' => t('The hostname of the PHPBB database server'),
  );

  $form['db']['phpbb2drupal_db_dbname'] = array(
    '#type' => 'textfield',
    '#title' => t('Database name'),
    '#default_value' => variable_get('phpbb2drupal_db_dbname', $databases['default']['default']['database']),
    '#description' => t('Name of the database on host server'),
  );

  $form['db']['phpbb2drupal_db_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#default_value' => variable_get('phpbb2drupal_db_username', $databases['default']['default']['username']),
    '#description' => t('Username used to access database'),
  );

  $form['db']['phpbb2drupal_db_password'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#description' => t('Password to your phpbb database'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Configuration'),
  );

  return $form;
}

/**
 * Validate callback for the phpbb configure form.
 */
function phpbb2drupal_configure_form_validate($form, &$form_state) {

  if ($form_state['values']['phpbb2drupal_files_domain'] == '' &&
      $form_state['values']['phpbb2drupal_files_directory'] == '') {
    form_set_error('', t('You must set a domain or directory to find forum files.'));
  }

  if ($form_state['values']['phpbb2drupal_db_local'] == 0 &&
      $form_state['values']['phpbb2drupal_db_host'] == '' &&
      $form_state['values']['phpbb2drupal_db_dbname'] == '' &&
      $form_state['values']['phpbb2drupal_db_username'] == '' &&
      $form_state['values']['phpbb2drupal_db_password'] == '') {
    form_set_error('', t('All external database credentials must be set if
                          you are not using the local database.'));
  }

  $domain = $form_state['values']['phpbb2drupal_files_domain'];
  if (drupal_substr($domain, -1) != '/') {
      $domain .= '/';
  }
  if (drupal_strlen($domain) > 0 && drupal_substr($domain, 0, 7) != 'http://') {
    $domain = "http://{$domain}";
  }
  $form_state['values']['phpbb2drupal_files_domain'] = $domain;
}

/**
 * Submit callback for the phpbb configure form.
 */
function phpbb2drupal_configure_form_submit($form, &$form_state) {
 // TODO: add variables to the registerMigrations raither than setting variables.
  // name phpbb2drupal_CUSTOM NAME_import name
   $settings['name'] = $form_state['values']['phpbb2drupal_import_name'];
   $settings['language'] = $form_state['values']['phpbb2drupal_language'];
   $settings['text_format']= $form_state['values']['phpbb2drupal_text_format'];
    
   $settings['db']['prefix'] = $form_state['values']['phpbb2drupal_db_prefix'];
   $settings['db']['local'] = $form_state['values']['phpbb2drupal_db_local'];
   $settings['db']['driver'] = $form_state['values']['phpbb2drupal_db_driver'];
   $settings['db']['host'] = $form_state['values']['phpbb2drupal_db_host'];
   $settings['db']['database'] = $form_state['values']['phpbb2drupal_db_dbname'];
   $settings['db']['username'] = $form_state['values']['phpbb2drupal_db_username'];
   $settings['files']['domain'] = $form_state['values']['phpbb2drupal_files_domain'];
   $settings['files']['directory'] = $form_state['values']['phpbb2drupal_files_directory'];
  if ($form_state['values']['phpbb2drupal_db_password']) {
    $settings['db']['password'] = $form_state['values']['phpbb2drupal_db_password'];
  }
  PhpbbConfig::registerMigrations($settings);
  drupal_set_message(t('PHPBB configuration changes saved.'));
}
