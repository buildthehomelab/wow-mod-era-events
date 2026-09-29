-- mod-era-events: Legion Incursion creatures (tier 7).
--
-- Copies of the Blizzard creatures at 9500820-9500827 with this module's scripts instead of their
-- SmartAI (which belongs to the Hellfire Peninsula invasion). Levels brought down to 60-62, the era
-- before the Dark Portal opens. Entries must match src/events/LegionIncursion.cpp.
--
-- Written as copies through temporary tables so no column names of creature_template appear here;
-- the server's schema differs from AzerothCore master (creature.id vs id1).
--
-- Idempotent: safe to run again.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_template_resistance` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_template_movement` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;
DELETE FROM `creature_text` WHERE `CreatureID` BETWEEN 9500820 AND 9500827;

DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_map`;
CREATE TEMPORARY TABLE `tmp_era_legion_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL, `script` VARCHAR(64) NOT NULL, `minlevel` TINYINT UNSIGNED NOT NULL, `maxlevel` TINYINT UNSIGNED NOT NULL, `health` FLOAT NOT NULL, `faction` SMALLINT UNSIGNED NOT NULL);
INSERT INTO `tmp_era_legion_map` VALUES
(19291, 9500820, 'npc_era_anchor', 0, 0, 0, 0), -- Legion Transporter: Alpha: the event anchor, invisible
(19284, 9500821, 'npc_era_event_mob', 60, 60, 0, 0), -- Invading Felguard
(19285, 9500822, 'npc_era_event_mob', 60, 61, 0, 0), -- Invading Infernal
(19286, 9500823, 'npc_era_event_mob', 60, 60, 0, 0), -- Invading Fel Stalker
(19287, 9500824, 'npc_era_event_mob', 60, 60, 0, 0), -- Invading Voidwalker
(19290, 9500825, 'npc_era_event_mob', 60, 61, 0, 0), -- Invading Anguisher
(8716, 9500826, 'npc_era_event_mob', 61, 61, 6, 0), -- Dreadlord: leads the third wave
(18945, 9500827, 'npc_era_event_mob', 62, 62, 25, 0); -- Pit Commander: comes through last

-- creature_template: copy, give it our script, drop SmartAI, set the era's level. The
-- entry changes in its own statement: a multi-table UPDATE doesn't order its assignments.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_ct`;
CREATE TEMPORARY TABLE `tmp_era_legion_ct` SELECT `ct`.* FROM `creature_template` `ct` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `ct`.`entry`;
UPDATE `tmp_era_legion_ct` `ct` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `ct`.`entry` SET
  `ct`.`AIName` = '',
  `ct`.`ScriptName` = `m`.`script`,
  `ct`.`minlevel` = IF(`m`.`minlevel`, `m`.`minlevel`, `ct`.`minlevel`),
  `ct`.`maxlevel` = IF(`m`.`maxlevel`, `m`.`maxlevel`, `ct`.`maxlevel`),
  `ct`.`HealthModifier` = IF(`m`.`health`, `m`.`health`, `ct`.`HealthModifier`),
  `ct`.`faction` = IF(`m`.`faction`, `m`.`faction`, `ct`.`faction`),
  `ct`.`exp` = IF(`m`.`maxlevel` AND `m`.`maxlevel` <= 62, 0, `ct`.`exp`);
UPDATE `tmp_era_legion_ct` `ct` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `ct`.`entry` SET `ct`.`entry` = `m`.`dst`;
INSERT INTO `creature_template` SELECT * FROM `tmp_era_legion_ct`;
DROP TEMPORARY TABLE `tmp_era_legion_ct`;

-- The rows that hang off creature_template: models, auras, weapons, resistances, spells, movement.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_template_model` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_model` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_template_addon` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`entry`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`entry` SET `x`.`entry` = `m`.`dst`;
INSERT INTO `creature_template_addon` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_equip_template` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_equip_template` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_template_resistance` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_resistance` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_template_spell` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_spell` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_legion_x`;
CREATE TEMPORARY TABLE `tmp_era_legion_x` SELECT `x`.* FROM `creature_template_movement` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_legion_x` `x` JOIN `tmp_era_legion_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_movement` SELECT * FROM `tmp_era_legion_x`;
DROP TEMPORARY TABLE `tmp_era_legion_x`;

DROP TEMPORARY TABLE `tmp_era_legion_map`;
