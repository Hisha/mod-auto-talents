-- mod-auto-talents
-- World database - complete base schema and initial build definitions.
--
-- Long-term shipped SQL policy:
--   * Complete CREATE TABLE definitions.
--   * Seed changes use DELETE/INSERT.
--   * No ALTER TABLE migrations are retained in base files.

CREATE TABLE IF NOT EXISTS `auto_talent_build` (
  `id` INT UNSIGNED NOT NULL,
  `class_id` TINYINT UNSIGNED NOT NULL,
  `name` VARCHAR(64) NOT NULL,
  `description` VARCHAR(255) NOT NULL DEFAULT '',
  `enabled` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_auto_talent_build_class_name` (`class_id`, `name`),
  KEY `idx_auto_talent_build_class_enabled` (`class_id`, `enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `auto_talent_build_step` (
  `build_id` INT UNSIGNED NOT NULL,
  `sequence` SMALLINT UNSIGNED NOT NULL,
  `talent_id` INT UNSIGNED NOT NULL,
  `rank` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`build_id`, `sequence`),
  KEY `idx_auto_talent_build_step_talent` (`talent_id`),
  CONSTRAINT `fk_auto_talent_build_step_build`
    FOREIGN KEY (`build_id`) REFERENCES `auto_talent_build` (`id`)
    ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Initial Paladin definitions used to prove selection and dual-spec assignment.
-- Ordered talent steps are intentionally added in the talent-engine milestone.
DELETE FROM `auto_talent_build_step` WHERE `build_id` IN (201, 202);
DELETE FROM `auto_talent_build` WHERE `id` IN (201, 202);

INSERT INTO `auto_talent_build` (`id`, `class_id`, `name`, `description`, `enabled`) VALUES
(201, 2, 'Protection', 'Paladin Protection leveling build', 1),
(202, 2, 'Retribution', 'Paladin Retribution leveling build', 1);
