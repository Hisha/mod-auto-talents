-- mod-auto-talents
-- Characters database - complete current schema.
-- Shipped schema is intentionally full-state CREATE/seed SQL; no permanent ALTER chain.

CREATE TABLE IF NOT EXISTS `auto_talent_character` (
  `guid` INT UNSIGNED NOT NULL,
  `spec_slot` TINYINT UNSIGNED NOT NULL,
  `build_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=prebuilt, 1=personal',
  `build_id` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'World build id when build_type=0; zero for personal',
  PRIMARY KEY (`guid`, `spec_slot`),
  KEY `idx_auto_talent_character_build` (`build_type`, `build_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `auto_talent_personal_build` (
  `guid` INT UNSIGNED NOT NULL,
  `spec_slot` TINYINT UNSIGNED NOT NULL,
  `class_id` TINYINT UNSIGNED NOT NULL,
  `name` VARCHAR(64) NOT NULL,
  `save_count` INT UNSIGNED NOT NULL DEFAULT 0,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`, `spec_slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `auto_talent_personal_build_step` (
  `guid` INT UNSIGNED NOT NULL,
  `spec_slot` TINYINT UNSIGNED NOT NULL,
  `sequence` SMALLINT UNSIGNED NOT NULL,
  `talent_name` VARCHAR(96) NOT NULL,
  `rank` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `spec_slot`, `sequence`),
  KEY `idx_auto_talent_personal_step_owner` (`guid`, `spec_slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

