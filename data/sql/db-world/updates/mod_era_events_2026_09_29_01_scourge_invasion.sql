-- mod-era-events: Scourge Invasion creatures (tier 6).
--
-- Copies of the Blizzard creatures at 9500800-9500809, with this module's scripts instead of the
-- core's: the originals' scripts write the global Scourge Invasion state or run SmartAI meant for
-- the world event. Levels are brought down to the era (players are 60 before TBC). Entries must
-- match src/events/ScourgeInvasion.cpp. Loot stays the originals' (same lootid).
--
-- Written as copies through temporary tables so no column names of creature_template appear here;
-- the server's schema differs from AzerothCore master (creature.id vs id1).
--
-- Idempotent: safe to run again.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_template_resistance` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_template_movement` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;
DELETE FROM `creature_text` WHERE `CreatureID` BETWEEN 9500800 AND 9500809;

DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_map`;
CREATE TEMPORARY TABLE `tmp_era_si_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL, `script` VARCHAR(64) NOT NULL, `minlevel` TINYINT UNSIGNED NOT NULL, `maxlevel` TINYINT UNSIGNED NOT NULL, `health` FLOAT NOT NULL);
INSERT INTO `tmp_era_si_map` VALUES
(16995, 9500800, 'npc_era_anchor', 0, 0, 0), -- Herald of the Lich King: the event anchor, invisible
(16394, 9500801, 'npc_era_pallid_horror', 62, 62, 20), -- Pallid Horror
(16382, 9500802, 'npc_era_pallid_horror', 62, 62, 20), -- Patchwork Terror
(16383, 9500803, 'npc_era_event_mob', 60, 60, 3), -- Flameshocker
(14697, 9500804, 'npc_era_event_mob', 61, 61, 0), -- Lumbering Horror
(16379, 9500805, 'npc_era_event_mob', 61, 61, 0), -- Spirit of the Damned
(16380, 9500806, 'npc_era_event_mob', 61, 61, 0), -- Bone Witch
(16141, 9500807, 'npc_era_event_mob', 58, 60, 0), -- Ghoul Berserker
(16298, 9500808, 'npc_era_event_mob', 58, 60, 0), -- Spectral Soldier
(16299, 9500809, 'npc_era_event_mob', 58, 60, 0); -- Skeletal Shocktrooper

-- creature_template: copy, give it our script, drop SmartAI, set the era's level. The
-- entry changes in its own statement: a multi-table UPDATE doesn't order its assignments.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_ct`;
CREATE TEMPORARY TABLE `tmp_era_si_ct` SELECT `ct`.* FROM `creature_template` `ct` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `ct`.`entry`;
UPDATE `tmp_era_si_ct` `ct` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `ct`.`entry` SET
  `ct`.`AIName` = '',
  `ct`.`ScriptName` = `m`.`script`,
  `ct`.`minlevel` = IF(`m`.`minlevel`, `m`.`minlevel`, `ct`.`minlevel`),
  `ct`.`maxlevel` = IF(`m`.`maxlevel`, `m`.`maxlevel`, `ct`.`maxlevel`),
  `ct`.`HealthModifier` = IF(`m`.`health`, `m`.`health`, `ct`.`HealthModifier`),
  `ct`.`exp` = IF(`m`.`maxlevel` AND `m`.`maxlevel` <= 62, 0, `ct`.`exp`);
UPDATE `tmp_era_si_ct` `ct` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `ct`.`entry` SET `ct`.`entry` = `m`.`dst`;
INSERT INTO `creature_template` SELECT * FROM `tmp_era_si_ct`;
DROP TEMPORARY TABLE `tmp_era_si_ct`;

-- The rows that hang off creature_template: models, auras, weapons, resistances, spells, movement.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_template_model` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_model` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_template_addon` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`entry`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`entry` SET `x`.`entry` = `m`.`dst`;
INSERT INTO `creature_template_addon` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_equip_template` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_equip_template` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_template_resistance` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_resistance` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_template_spell` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_spell` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.* FROM `creature_template_movement` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` `x` JOIN `tmp_era_si_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_movement` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;

-- Voice lines. Range 0 (yell distance) instead of zone-wide: yells respect phases, zone
-- messages would reach players of every era.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_text_map`;
CREATE TEMPORARY TABLE `tmp_era_si_text_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL);
INSERT INTO `tmp_era_si_text_map` VALUES (16995, 9500800), (16394, 9500801), (16394, 9500802);
DROP TEMPORARY TABLE IF EXISTS `tmp_era_si_x`;
CREATE TEMPORARY TABLE `tmp_era_si_x` SELECT `x`.*, `m`.`dst` AS `era_dst` FROM `creature_text` `x` JOIN `tmp_era_si_text_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_si_x` SET `CreatureID` = `era_dst`, `TextRange` = 0;
ALTER TABLE `tmp_era_si_x` DROP COLUMN `era_dst`;
INSERT INTO `creature_text` SELECT * FROM `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_x`;
DROP TEMPORARY TABLE `tmp_era_si_text_map`;

DROP TEMPORARY TABLE `tmp_era_si_map`;
