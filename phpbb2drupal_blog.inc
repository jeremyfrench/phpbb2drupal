<?php

/**
 * @file
 * Import entries from the PHPBB Blog Mod
 */
class PhpbbBlogMigration extends PhpbbMigration {

  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate individual blog entries from phpbb.');
    $this->dependencies = array('PhpbbUser');

    $this->source = new MigrateSourceSQL(
      Database::getConnection('default', 'phpbb')
        ->select('{blogs}', 'b')
        ->fields('b', array(
          'blog_id', 'user_id', 'blog_subject', 'blog_text',
          'blog_time', 'blog_edit_time', 'bbcode_uid'
        )
      )
    );

    $this->destination = new MigrateDestinationNode('blog',
      array('text_format' => $this->importFormat)
    );

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'blog_id' => array('type' => 'int', 'unsigned' => TRUE, 'not null' => TRUE)
      ),
      MigrateDestinationNode::getKeySchema(), 'phpbb'
    );

    $this->addFieldMapping('uid', 'user_id')
      ->sourceMigration('PhpbbUser')->defaultValue(1);
    $this->addFieldMapping('title', 'blog_subject');
    $this->addFieldMapping('body', 'blog_text');
    $this->addFieldMapping('status', 'post_approved');
    $this->addFieldMapping('created', 'blog_time');
    $this->addFieldMapping('changed', 'blog_edit_time');

    $this->addFieldMapping(NULL, 'bbcode_uid')->issueGroup(t('DNM')); // Used to santise body text

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array(
      'is_new', 'revision', 'revision_uid', 'promote', 'sticky', 'language', 'comment', 'path'
    ));
  }

  function prepareRow($post) {
    // remove the :bbcode_uid from post_text
    if (!empty($post->bbcode_uid)) {
      $post->blog_text = preg_replace("/:$post->bbcode_uid/", '', $post->blog_text);
    }
    $post->blog_text = PhpbbContentHelpers::stripBbcode($post->blog_text);
    $post->blog_text = PhpbbContentHelpers::textSanitise($post->blog_text);
    $post->blog_subject = PhpbbContentHelpers::textSanitise($post->blog_subject);
  }
}

