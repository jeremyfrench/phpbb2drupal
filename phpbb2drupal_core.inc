<?php

include_once('phpbb2drupal.inc');

/**
 * Migrate users from phpbb to Drupal
 *
 * @author DarkSnow.net
 * @file
 * Migrate all the core functionality of a PHPBB forum into Drupal Core
 *
 * @issue http://drupal.org/node/777168
 * On migration we'll get a warning:
 *  "Undefined property: stdClass::$uid in profile_user_presave()"
 *  for every user. This is linked to an issue with profile and should be resolved soon.
 */
class PhpbbUserMigration extends PhpbbMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate phpbb users to Drupal users');

    $this->dependencies = array('PhpbbAvatar');

    // Make sure Forums import appears first in the list even though we
    // can run the user import without the forums in place.
    $this->softDependencies = array('PhpbbForums');

    // Select everything from the phpbb users table
    $this->source = new MigrateSourceSQL(
      Database::getConnection('default', 'phpbb')
        ->select('{users}', 'u')->fields('u', array(
         'user_id', 'user_type', 'username', 'user_password', 'user_email', 'user_sig', 'user_sig_bbcode_uid',
         'user_regdate', 'user_lastvisit', 'user_inactive_reason',
         'group_id', 'user_timezone', 'user_avatar', 'user_website', 'user_from', 'user_birthday',
         'user_icq', 'user_aim', 'user_yim', 'user_msnm', 'user_jabber',
         'user_occ', 'user_interests'))
        ->condition('user_type', 2, '<>') // User type 2 is for anonymous and web crawlers
        ->condition('user_id', 2, '>') // Skip Anonymous and Admin users
    );

    // Built in destination for users
    $this->destination = new MigrateDestinationUser(MigrateDestinationUser::options(
      $this->defaultLanguage, $this->importFormat, FALSE
    ));

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'user_id' => array('type' => 'int', 'unsigned' => TRUE, 'not null' => TRUE)
      ),
      MigrateDestinationUser::getKeySchema(),  'phpbb'
    );

    $this->addFieldMapping('name', 'username');
//    $this->addFieldMapping('pass', 'user_password'); // Password handled in complete()
    $this->addFieldMapping('mail', 'user_email');
    $this->addFieldMapping('signature', 'user_sig');
    $this->addFieldMapping('created', 'user_regdate');
    $this->addFieldMapping('access', 'user_lastvisit');
    $this->addFieldMapping('login', 'user_lastvisit');

    // User status handled in prepare()
    $this->addFieldMapping(NULL, 'user_type')->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'user_inactive_reason')->issueGroup(t('DNM'));

    $this->addFieldMapping('timezone', 'user_timezone');
    $this->addFieldMapping('init', 'user_email');
    $this->addFieldMapping('roles')->defaultValue(2); # Authenticated User
    $this->addFieldMapping('user_website', 'user_website');
    $this->addFieldMapping('user_from', 'user_from');
    $this->addFieldMapping('user_occ', 'user_occ');
    $this->addFieldMapping('user_interests', 'user_interests');
    $this->addFieldMapping('signature_format')->defaultValue($this->importFormat);
    $this->addFieldMapping('language')->defaultValue($this->defaultLanguage);

    // Add all IM fields to IM Contacts field, done in prepareRow
    $this->addFieldMapping(NULL, 'user_icq')->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'user_aim')->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'user_yim')->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'user_msnm')->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'user_jabber')->issueGroup(t('DNM'));
    $this->addFieldMapping('user_im', 'user_im')->separator(PHPBB_LIST_SEPARATOR);

    $this->addFieldMapping('picture', 'user_avatar')->sourceMigration('PhpbbAvatar');

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array(
      'path', 'is_new', 'theme',
      'status', // Handled in prepare
    ));

  }

  /**
   * Preprocess import rows before any other processing occurs
   *
   * @param $user The current import row
   */
  public function prepareRow($user) {
    $user->user_aim = strtr($user->user_aim, array('+' => ' ')); # PHPBB stores spaces as +, replace with ' '
    $user->user_yim = strtr($user->user_yim, array('+' => ' '));

    // Concatenate all IM fields into the user_im field so the system will add them all to one field
    $im_contacts = array();
    if (!empty($user->user_icq)   ) array_push($im_contacts, 'ICQ: ' . $user->user_icq);
    if (!empty($user->user_aim)   ) array_push($im_contacts, 'AOL: ' . $user->user_aim);
    if (!empty($user->user_yim)   ) array_push($im_contacts, 'Yahoo: ' . $user->user_yim);
    if (!empty($user->user_msnm)  ) array_push($im_contacts, 'MSNM: ' . $user->user_msnm);
    if (!empty($user->user_jabber)) array_push($im_contacts, 'Jabber: ' . $user->user_jabber);
    $user->user_im = count($im_contacts) ? implode(PHPBB_IM_SEPARATOR, $im_contacts) : NULL;

    $user->user_timezone = $user->user_timezone * 60 * 60; # Drupal stores timezones in seconds

    // remove the :bbcode_uid from signature
    if (!empty($user->user_sig)) {
      $user->user_sig = preg_replace("/:$user->user_sig_bbcode_uid/", '', $user->user_sig);
    }
    $user->user_sig = PhpbbContentHelpers::textSanitise(PhpbbContentHelpers::stripBbcode($user->user_sig));

    $user->user_sig = drupal_substr($user->user_sig, 0, 255); # Limit signature to 255 characters

    if (!empty($user->user_avatar)) {
      $user->user_avatar = PhpbbAvatarMigration::getRealFilename($user->user_avatar);
    }
  }

  public function prepare($user, stdClass $row) {
    // If the user was marked as inactive for any reason, keep them inactive in Drupal
    $user->status = ($row->user_inactive_reason == 0 && $row->user_type <> 1) ? 1 : 0;
  }

  public function complete($user, stdClass $row) {
    // Copy the password directly into the use table so we can use existing passwords
    // UPDATE users SET pass = $row->user_password WHERE uid = $user->uid
    db_update('{users}')
      ->condition('uid', $user->uid)
      ->fields(array('pass' => $row->user_password))
      ->execute();
  }
}

/**
 * Import forum user avatars from phpbb to core picture field
 */
class PhpbbAvatarMigration extends PhpbbMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate user avatars from phpbb.');

    $this->map = new MigrateSQLMap($this->machineName,
      array('user_avatar' => array(
            'type' => 'varchar',
            'length' => 255,
            'not null' => TRUE,
            'description' => 'Avatar File Name.'
        )
      ),
      MigrateDestinationFile::getKeySchema(), 'phpbb'
    );

    $query = Database::getConnection('default', 'phpbb')
      ->select('{users}', 'u')
      ->fields('u', array('user_avatar'))
      ->isNotNull('user_avatar')
      ->condition('user_avatar', "", '<>');
    $this->source = new MigrateSourceSQL($query);

    // TIP: Set copy_file to copy the file from its source (which could be a
    // remote server, i.e. the uri is of the form http://example.com/images/foo.jpg).
// TODO: Strip full phpbb path from the filename so files are copied into pictures directory
    $this->destination = new MigrateDestinationFile(array(
      'copy_file' => TRUE,
      'copy_destination' => file_build_uri(variable_get('user_picture_path', 'pictures')) . '/'
    ));

    // Just map the incoming URL to the destination's 'uri'
    $this->addFieldMapping('uri', 'user_avatar');
    $this->addFieldMapping('status')->defaultValue(1);

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array('filename', 'contents', 'path'));
  }

  /**
   * Preprocess import rows before any other processing occurs
   *
   * @param $user The current import row
   */
  public function prepareRow($avatar) {
    if (!empty($avatar->user_avatar)) {
      $avatar->user_avatar = self::getRealFilename($avatar->user_avatar);
    }
  }

  static public function getRealFilename($dbName) {
    $name = pathinfo($dbName);
    $pos = strpos($name['basename'], '_');

    $filename = PhpbbConfig::getPath('avatar') . '/' . PhpbbConfig::getVar('avatar_salt') . '_'
      . drupal_substr($name['basename'], 0, $pos) . '.' . $name['extension'];

    // The rest of the db_name is actually the timestamp and can be got like this
    // $avatar->timestamp = substr($name['filename'], $pos + 1);
    return $filename;
  }
}

/**
 * Import hierarchy of forums from phpbb to core forum.
 * Core forum uses vocabulary terms, under the Forums vocabulary, to
 * create the hierarchy of forums.
 */
class PhpbbForumsMigration extends PhpbbMigration {
  protected $forum_weight = 0;
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate forum structure from phpbb.');

    /**
     * Get the bits we need, the sort order ensures root level parents appear
     * first and that we get the children in the correct order.
     */
    $this->source = new MigrateSourceSQL(
      Database::getConnection('default', 'phpbb')
      ->select('{forums}', 'f')
      ->fields('f', array('forum_id', 'parent_id', 'forum_name', 'forum_desc', 'forum_type'))
      ->condition('forum_type', 2, '<>')
      ->orderBy('parent_id')
      ->orderBy('right_id')
      ->orderBy('left_id')
    );

    // Built in destination for Taxonomy Terms
    $this->destination = new MigrateDestinationTerm('forums');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'forum_id' => array('type' => 'int', 'unsigned' => TRUE, 'not null' => TRUE)
      ),
      MigrateDestinationTerm::getKeySchema(), 'phpbb'
    );
    $this->addFieldMapping('name', 'forum_name');
    $this->addFieldMapping('description', 'forum_desc');
    $this->addFieldMapping('parent', 'parent_id')->sourceMigration('PhpbbForums');
    $this->addFieldMapping('format')->defaultValue($this->importFormat);

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array('weight', 'parent_name', 'path'));
  }

  /**
   * Ensure root level (parent == 0) forums don't get a stub parent created
   *
   * @param Migration $migration The current running migration
   */
  protected function createStub($migration) {
    if ($migration->sourceValues->parent_id == 0) {
      return array(0);
    }

    $tax = new stdClass;
    $tax->name = t('Stub');
    $tax->description = $migration->sourceValues->forum_desc;
//  TODO Find out how the destination knows the vid instead of getting the variable
    $tax->vid = variable_get('forum_nav_vocabulary', 0);
    taxonomy_term_save($tax);
    if (isset($tax->tid)) {
      return array($tax->tid);
    }
    else {
      return FALSE;
    }
  }

  /**
   * Increment and use the objects weight variable so the ordering
   * of the query will set the weight, and thus order, of the taxonomy
   *
   * @param TaxonomyTerm $forum The Destination taxonomy term
   * @param stdClass     $row   The current source row
   */
  public function prepare($forum, stdClass $row) {
    $forum->weight = ++$this->forum_weight;
  }

  /**
   * Once the forum term has been imported, if it's a container, which is
   * forum_type == 0 in phpbb, mark it as such in Drupal
   *
   * @param TaxonomyTerm $forum The newly imported forum
   * @param stdClass     $row   The current source row
   */
  public function complete($forum, stdClass $row) {
    if ($row->forum_type == 0) {
      $containers = variable_get('forum_containers', array());
      array_push($containers, $forum->tid);
      variable_set('forum_containers', $containers);
    }
  }
}

/**
 * Import forum topics from phpbb to core forum nodes
 */
class PhpbbTopicMigration extends PhpbbMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate individual forum topics from phpbb.');
    $this->dependencies = array('PhpbbUser', 'PhpbbForums');

    $query = Database::getConnection('default', 'phpbb')->select('{posts}', 'p');
    $query->leftJoin('{topics}', 't', 'p.topic_id = t.topic_id');
    $this->source = new MigrateSourceSQL(
      $query->fields('p', array(
        'forum_id', 'post_id', 'poster_id', //'poster_ip',
        'post_time', 'post_edit_time', 'post_approved',
        'post_subject', 'post_text', 'bbcode_uid'
      ))
      ->condition('post_approved', 1)
      ->where('p.post_id = t.topic_first_post_id')
      ->orderBy('post_time')
    );
    
    $this->destination = new MigrateDestinationNode('forum',
      array('text_format' => $this->importFormat)
    );

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'post_id' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'alias' => 'p'
        )
      ),
      MigrateDestinationNode::getKeySchema(), 'phpbb'
    );

    // Put NULL in here due to error http://drupal.org/node/839770
    $this->addFieldMapping('nid')->defaultValue(NULL);
    $this->addFieldMapping('uid', 'poster_id')
      ->sourceMigration('PhpbbUser')->defaultValue(1);
    $this->addFieldMapping('taxonomy_forums', 'forum_id')
      ->sourceMigration('PhpbbForums')
      ->arguments(array('source_type' => 'tid'));
//    $this->addFieldMapping('hostname', 'poster_ip');
    $this->addFieldMapping('status', 'post_approved');
    $this->addFieldMapping('title', 'post_subject');
    $this->addFieldMapping('body', 'post_text');
    $this->addFieldMapping('created', 'post_time');
    $this->addFieldMapping('changed', 'post_edit_time');
    $this->addFieldMapping('language')->defaultValue($this->defaultLanguage);

    $this->addFieldMapping(NULL, 'bbcode_uid')->issueGroup(t('DNM')); // Used to santise body text

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array(
      'is_new', 'revision', 'revision_uid', 'promote', 'sticky', 'comment', 'path', 'pathauto'
    ));
  }

  function prepareRow($post) {
    // remove the :bbcode_uid from post_text
    if (!empty($post->bbcode_uid)) {
      $post->post_text = preg_replace("/:$post->bbcode_uid/", '', $post->post_text);
    }
    $post->post_text = PhpbbContentHelpers::stripBbcode($post->post_text);
    $post->post_text = PhpbbContentHelpers::textSanitise($post->post_text);
    $post->post_subject = PhpbbContentHelpers::textSanitise($post->post_subject);
  }
}

/**
 * Import forum posts from phpbb to core forum comments
 */
class PhpbbPostMigration extends PhpbbMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate individual forum posts from phpbb.');
    $this->dependencies = array('PhpbbTopic');

    $query = Database::getConnection('default', 'phpbb')->select('{posts}', 'p');
    $query->leftJoin('{topics}', 't', 'p.topic_id = t.topic_id');
    $query->addField('t', 'topic_first_post_id');
    $this->source = new MigrateSourceSQL(
      $query->fields('p', array(
        'post_id', 'poster_id', 'poster_ip',
        'post_time', 'post_edit_time', 'post_approved',
        'post_subject', 'post_text', 'bbcode_uid'
      ))
      ->condition('post_approved', 1)
      ->where('p.post_id <> t.topic_first_post_id')
      ->orderBy('post_time')
    );

    $this->destination = new MigrateDestinationComment('comment_node_forum',
      array('text_format' => $this->importFormat)
    );

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'post_id' => array('type' => 'int', 'unsigned' => TRUE, 'not null' => TRUE)
      ),
      MigrateDestinationComment::getKeySchema(), 'phpbb'
    );

    $this->addFieldMapping('uid', 'poster_id')
      ->sourceMigration('PhpbbUser')->defaultValue(1);
    $this->addFieldMapping('nid', 'topic_first_post_id')
      ->sourceMigration('PhpbbTopic');
    $this->addFieldMapping('hostname', 'poster_ip');
    $this->addFieldMapping('status', 'post_approved');
    $this->addFieldMapping('subject', 'post_subject');
    $this->addFieldMapping('comment_body', 'post_text');
    $this->addFieldMapping('created', 'post_time');
    $this->addFieldMapping('changed', 'post_edit_time');
    $this->addFieldMapping('language')->defaultValue($this->defaultLanguage);

    $this->addFieldMapping(NULL, 'bbcode_uid')->issueGroup(t('DNM')); // Used to santise body text

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array(
      'pid', 'path', 'thread', 'name', 'mail', 'homepage'
    ));
  }

  function prepareRow($post) {
    // remove the :bbcode_uid from post_text
    if (!empty($post->bbcode_uid)) {
      $post->post_text = preg_replace("/:$post->bbcode_uid/", '', $post->post_text);
    }
    $post->post_text = PhpbbContentHelpers::stripBbcode($post->post_text);
    $post->post_text = PhpbbContentHelpers::textSanitise($post->post_text);
    $post->post_subject = PhpbbContentHelpers::textSanitise($post->post_subject);
  }
}

/**
 * Import attachemtns from phpbb to Drupal file fields
 */
class PhpbbAttachmentMigration extends PhpbbMigration {
  // TODO: Automatically rename the files on disk before starting. Currently done by drush command.
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate all phpbb attachments to a file field on the relavant D7 entity.');

    $this->dependencies = array('PhpbbPost');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'attach_id' => array('type' => 'int', 'unsigned' => TRUE, 'not null' => TRUE)
      ),
      MigrateDestinationFile::getKeySchema(), 'phpbb'
    );

    $query = Database::getConnection('default', 'phpbb')
      ->select('{attachments}', 'a')
      ->fields('a', array(
        'attach_id', 'post_msg_id', 'topic_id', 'poster_id',
        'real_filename', //'physical_filename',
        'attach_comment', 'filetime'));

    $query->innerJoin('{topics}', 't', 'a.topic_id = t.topic_id');
    $query->addField('t', 'topic_first_post_id');

    $this->source = new MigrateSourceSQL($query);

// TODO: Strip full phpbb path from the filename so files are copied into attachments directory
    $this->destination = new MigrateDestinationFile(array(
      'copy_file' => TRUE,
      // TODO: Get this directory from the Attachments field settings
      'copy_destination' => file_build_uri('attachments') . '/'
    ));

    // Just map the incoming URL to the destination's 'uri'
    $this->addFieldMapping('uri', 'real_filename');
    $this->addFieldMapping('uid', 'poster_id')->sourceMigration('PhpbbUser');
    $this->addFieldMapping('timestamp', 'filetime');
    $this->addFieldMapping('description', 'attach_comment');
    $this->addFieldMapping('display')->defaultValue(1);
    $this->addFieldMapping('topic_id', 'topic_id')->sourceMigration('PhpbbTopic');
    $this->addFieldMapping('post_id', 'post_msg_id')->sourceMigration('PhpbbPost');
    $this->addFieldMapping(NULL, 'topic_first_post_id')->issueGroup(t('DNM'));

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array('filename', 'filemime', 'contents', 'status', 'path'));

  }
  /**
   * Preprocess import rows before any other processing occurs
   *
   * @param $user The current import row
   */
  public function prepareRow($att) {
    $att->real_filename = PhpbbConfig::getPath('upload') . "/{$att->real_filename}";
  }

  /**
   * Once the file has been imported, rename it and
   * associate it with the appropriate node
   */
  public function complete($file, stdClass $row) {
    $i = 0;
    $file->display = 1;
    $file->description = $row->attach_comment;
    if ($row->topic_first_post_id == $row->post_msg_id) {
      // It's a topic
      $topic = node_load($this->destinationValues->topic_id);
      field_attach_load('forum', array($topic));
      if (count($topic->field_attach)) {
        $i = count($topic->field_attach[LANGUAGE_NONE]);
      }
      $topic->field_attach[LANGUAGE_NONE][$i] = (array)$file;

      node_submit($topic);
      node_save($topic);
    }
    else {
      // It's a comment
      $post = comment_load($this->destinationValues->post_id);
      field_attach_load('comment_node_forum', array($post));
      if (count($post->field_attach)) {
        $i = count($post->field_attach[LANGUAGE_NONE]);
      }
      $post->field_attach[LANGUAGE_NONE][$i] = (array)$file;
      $post->is_anonymous = 0;

      comment_submit($post);
      comment_save($post);
    }
  }
}
