-- mod-auto-talents
-- Characters database - complete base schema.

CREATE TABLE IF NOT EXISTS `auto_talent_character` (
  `guid` INT UNSIGNED NOT NULL,
  `spec_slot` TINYINT UNSIGNED NOT NULL,
  `build_id` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `spec_slot`),
  KEY `idx_auto_talent_character_build` (`build_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
