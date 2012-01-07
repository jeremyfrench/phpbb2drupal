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

  public function __construct() {
    global $language;
    parent::__construct(MigrateGroup::getInstance('PhpBB'));

    $this->importFormat = PhpbbConfig::getVar('text_format', '');
    $this->defaultLanguage = $language->language;

    $this->team = array(
      new MigrateTeamMember('Martin Fraser', 'mdf@darksnow.net', t('Implementor'))
    );

    // Individual mappings in a migration can be linked to a ticket or issue
    // in an external tracking system. Define the URL pattern here in the shared
    // class with ':id:' representing the position of the issue number, then add
    // ->issueNumber(1234) to a mapping.
    $this->issuePattern = 'http://drupal.org/node/:id:';

    PhpbbConfig::createConnection();
  }
}

/**
 * Static class for content helpers
 */
class PhpbbContentHelpers {
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
   * Get a variable from the Phpbb config table, or NULL if not found.
   * This requires the database connection to have already been created.
   * @param $var
   */
  static function getPhpbbDbVar($var) {
    $ret = NULL;
    try {
      $result = Database::getConnection('default', 'phpbb')
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
   * Get a configuration variable. If it's from the migration config, get it
   * from system variables, otherwise get it from the phpbb config table
   * @param $var The variable requested
   * @param $def The default value to return if there's no match
   */
  static function getVar($var, $def = NULL) {
    $ret = NULL;
    switch ($var) {
      case 'text_format':
      case 'db_prefix':
      case 'db_local':
      case 'db_driver':
      case 'db_host':
      case 'db_dbname':
      case 'db_username':
      case 'db_password':
      case 'files_domain':
      case 'files_directory':
        $ret = variable_get("phpbb2drupal_{$var}", $def);
        break;
      case 'files_path':
        $ret = self::getVar('files_domain');
        if ($ret == '') {
          $ret = file_build_uri(self::getVar('files_directory'));
        }
        break;
      default:
        return self::getPhpbbDbVar($var);
        break;
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
  static function getPath($var) {
    return self::getVar('files_path') . self::getVar("{$var}_path");
  }

  /**
   * Get an array of database credentials compatible with
   * Database::addConnectionInfo()
   */
  static function getDbCredentials() {
    global $databases;
    $con;
    if (self::getVar('db_local', 1)) {
      $con = $databases['default']['default'];
    }
    else {
      $con = array(
        'driver' => self::getVar('db_driver'),
        'database' => self::getVar('db_dbname'),
        'username' => self::getVar('db_username'),
        'password' => self::getVar('db_password'),
        'host' => self::getVar('db_host')
      );
    }
    $con['prefix'] = self::getVar('db_prefix');
    return $con;
  }

  /**
   * Create a phpbb database connection if it's not already there.
   */
  static function createConnection() {
    global $databases;
    if (empty($databases['default']['phpbb'])) {
      Database::addConnectionInfo('phpbb', 'default', PhpbbConfig::getDbCredentials());
    }
  }

  /**
   * Register all migrations.
   * Call this AFTER the configuration has been done!
   */
  static function registerMigrations() {
    foreach (self::getMigrations() as $class => $mod) {
      if (module_exists($mod)) {
        Migration::registerMigration("{$class}Migration", $class);
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
    return array(
      'PhpbbAvatar' => 'phpbb2drupal',
      'PhpbbForums' => 'phpbb2drupal',
      'PhpbbUser' => 'phpbb2drupal',
      'PhpbbTopic' => 'phpbb2drupal',
      'PhpbbPost' => 'phpbb2drupal',
      'PhpbbAttachment' => 'phpbb2drupal',
      'PhpbbPrivateMessage' => 'phpbb2drupal_privatemsg',
      'PhpbbBlog' => 'phpbb2drupal_blog',
      //'PhpbbBlogAttachment' => 'phpbb2drupal_blog',
    );
  }
}
