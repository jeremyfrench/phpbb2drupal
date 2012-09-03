<?php

/**
 * @file
 * Core file of the migration, containing code helpful to all migrations
 */

/**
 * Used to build a multi value list
 */
define("PHPBB_LIST_SEPARATOR", '|');

abstract class PhpbbMigration extends DynamicMigration {

  protected $importFormat;
  protected $defaultLanguage;
  protected $migration_settings;
  static $phpbb_type = NULL;
  
  /**
   * Get a variable from the Phpbb config table, or NULL if not found.
   * This requires the database connection to have already been created.
   * @param $var
   */
  function getPhpbbDbVar($var) {
   $ret = NULL;
   try {
    $result = Database::getConnection('default', $this->migration_settings['machine_name'])
    ->select('{config}', 'c')
    ->fields('c', array('config_value'))
    ->condition('config_name', $var)
    ->execute()
    ->fetchAssoc();
    if ($result) {
     $ret = $result['config_value'];
    }
   }
   catch (Exception $ex) {
    // Assume an error means we won't find anything, so return the NULL
   }
   return $ret;
  }
  
  
  
  /**
   * Build a stream path for the request phpbb config variable
   * Valid paths are: avatar, icons, ranks, smilies, upload
   *
   * @param $var The variable requested, without the _path suffix
   * @return The complete URI of the requested path
   */
  function getPath($var) {
   return $this->getVar('files_path') . $this->getVar("{$var}_path");
  }
  
  // TODO: create method for classes with same settings to refere to each other.
  
  public function __construct($settings = array()) {
    $called_class = get_called_class();
    
    Database::addConnectionInfo($settings['machine_name'], 'default', $settings['db']);
    $this->migration_settings = $settings;
    //TODO: so that each set of migrations is in it's own group.
    parent::__construct(MigrateGroup::getInstance('PhpBB'));
    $this->machineName = $this->getMigrationName($called_class::$phpbb_type);
    
    $this->importFormat = $settings['text_format'];
    $this->defaultLanguage = $settings['language'];

    $this->team = array(
      new MigrateTeamMember('Martin Fraser', 'mdf@darksnow.net', t('Implementor')),
      new MigrateTeamMember('Jeremy French', 'jeremy@jeremyfrench.co.uk', t('i18n and multi import'))
    );

    // Individual mappings in a migration can be linked to a ticket or issue
    // in an external tracking system. Define the URL pattern here in the shared
    // class with ':id:' representing the position of the issue number, then add
    // ->issueNumber(1234) to a mapping.
    $this->issuePattern = 'http://drupal.org/node/:id:';

  }
  /**
   * Migrations which are part of the same set will have common names, this is a function for 
   * them to be able to generate their common names and reference each other for dependancies 
   * etc.
   * 
   * @param string $base_name the base type of the name for example user or privatemsg
   */
  protected function getMigrationName($base_name) {
    return 'phpbb_' . $this->migration_settings['machine_name'] . '_' . $base_name;
  }
}

/**
 * Static class for content helpers
 */
class PhpbbContentHelpers {
  /**
   * Strip the BBCode UID from the string
   *
   * @param unknown_type $text The source text to be changed
   * @param unknown_type $uid The BBCode UId to remove
   */
  static function stripUid($text, $uid) {
    return preg_replace("/(:[a-z])?:$uid\]/", ']', $text);
  }

  /**
   * Strips text of extra phpbb markup and if requested, also strips all bbcode from text.
   */
  static function stripBbcode($text) {
    // Strip the text of extra markup - regular expressions taken
    // from phpbb includes/function.php, function get_preg_expression().
    $match = array(
      '#<!\-\- e \-\-><a href="mailto:(.*?)">.*?</a><!\-\- e \-\->#',
      '#<!\-\- l \-\-><a (?:class="[\w-]+" )?href="(.*?)(?:(&amp;|\?)sid=[0-9a-f]{32})?">.*?</a><!\-\- l \-\->#',
      '#<!\-\- ([mw]) \-\-><a (?:class="[\w-]+" )?href="(.*?)">.*?</a><!\-\- \1 \-\->#',
      '#<!\-\- s(.*?) \-\-><img src="\{SMILIES_PATH\}\/.*? \/><!\-\- s\1 \-\->#',
      '#<!\-\- .*? \-\->#s',
      '#<.*?>#s',
    );
    $replace = array('$1', '$1', '$2', '$1', '', '');
    $text = preg_replace($match, $replace, $text);

    return $text;
  }

  /**
   * function to properly encode strings.
   */
  static function textSanitise($text) {
    $text = html_entity_decode($text, ENT_QUOTES, 'utf-8');
    return $text;
  }

}

/**
 * Class to simplify access to the configuration of PHPBB and this migration
 */
class PhpbbConfig {


  /**
   * Create a phpbb database connection if it's not already there.
   */
  static function createConnection($settings) {
    global $databases;
    if ($settings['db']['local']) {
     $settings['db'] = $databases['default']['default'];
    }
    if (empty($databases['default'][$settings['machine_name']])) {
      Database::addConnectionInfo($settings['machine_name'], 'default', $settings['db']);
    }
  }

  /**
   * Register all migrations.
   * Call this AFTER the configuration has been done!
   */
  static function registerMigrations($settings) {
   // Create machine name for settings
   $settings['machine_name'] = preg_replace('/[^A-Za-z0-9]/', '_', $settings['name']) . '_' . $settings['language'];
   foreach (self::getMigrations() as $class => $mod) {
      if (module_exists($mod)) {
        $type = $class::$phpbb_type;
        Migration::registerMigration($class, 'phpbb_' . $settings['machine_name'] . '_' . $type, $settings);
      }
    }
  }

  /**
   * Deregister all migrations.
   * Called when the module is disabled
   */
  static function deregisterMigrations($module) {
    foreach (self::getMigrations() as $machine_name => $mod) {
      if ($mod == $module) {
        // See if this migration is already registered
        $migration_row = db_select('migrate_status', 'ms')
          ->fields('ms', array('class_name', 'arguments'))
          ->condition('machine_name', $machine_name)
          ->execute()
          ->fetchObject();
        if ($migration_row) {
          Migration::deregisterMigration($machine_name);
        }
      }
    }
  }

  /**
   * Get a list of all migrations supported by this class.
   * Also provides a list of the sub modules that implement a given migration.
   */
  static function getMigrations() {
   $migrations = array(
      'PhpbbAvatarMigration' => 'phpbb2drupal',
      'PhpbbForumsMigration' => 'phpbb2drupal',
      'PhpbbUserMigration' => 'phpbb2drupal',
      'PhpbbTopicMigration' => 'phpbb2drupal',
      'PhpbbPostMigration' => 'phpbb2drupal',
      'PhpbbAttachmentMigration' => 'phpbb2drupal',
      'PhpbbPrivateMessageMigration' => 'phpbb2drupal_privatemsg',
      //'PhpbbBlogMigration' => 'phpbb2drupal_blog',
      //'PhpbbBlogAttachmentMigration' => 'phpbb2drupal_blog',
    );
   drupal_alter('phpbbmigrations', $migrations);
   return $migrations;
   
  }
}
