-- mod-era-events: Zombie Infestation (tier 12).
--
-- Copies of the 3.0.2 event's creatures at 9500850-9500853, which AzerothCore has templates for but
-- never spawns, and the Plagued Grain Crate as 9500854, made clickable (goober) for the module's
-- script. Entries must match src/events/ZombieInfestation.cpp.
--
-- Beckoning Groan (56560), on the zombie form's action bar, gets the script that spreads the
-- infection. Nothing else uses that spell.
--
-- Written as copies through temporary tables so no column names of creature_template or
-- gameobject_template appear here; the server's schema differs from AzerothCore master.
--
-- Idempotent: safe to run again.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_template_resistance` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_template_movement` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;
DELETE FROM `creature_text` WHERE `CreatureID` BETWEEN 9500850 AND 9500853;

DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_map`;
CREATE TEMPORARY TABLE `tmp_era_zombie_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL, `script` VARCHAR(64) NOT NULL, `minlevel` TINYINT UNSIGNED NOT NULL, `maxlevel` TINYINT UNSIGNED NOT NULL, `health` FLOAT NOT NULL, `faction` SMALLINT UNSIGNED NOT NULL);
INSERT INTO `tmp_era_zombie_map` VALUES
(15454, 9500850, 'npc_era_anchor', 0, 0, 0, 0), -- Anachronos Quest Trigger Invisible: the event anchor
(27059, 9500851, 'npc_era_event_mob', 70, 70, 1.5, 0), -- Plague Zombie
(27848, 9500852, 'npc_era_event_mob', 68, 70, 0.6, 0), -- Plagued Resident
(27305, 9500853, '', 70, 70, 0, 0); -- Argent Healer: cures infected players standing next to it

-- creature_template: copy, give it our script, drop SmartAI, set the era's level. The
-- entry changes in its own statement: a multi-table UPDATE doesn't order its assignments.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_ct`;
CREATE TEMPORARY TABLE `tmp_era_zombie_ct` SELECT `ct`.* FROM `creature_template` `ct` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `ct`.`entry`;
UPDATE `tmp_era_zombie_ct` `ct` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `ct`.`entry` SET
  `ct`.`AIName` = '',
  `ct`.`ScriptName` = `m`.`script`,
  `ct`.`minlevel` = IF(`m`.`minlevel`, `m`.`minlevel`, `ct`.`minlevel`),
  `ct`.`maxlevel` = IF(`m`.`maxlevel`, `m`.`maxlevel`, `ct`.`maxlevel`),
  `ct`.`HealthModifier` = IF(`m`.`health`, `m`.`health`, `ct`.`HealthModifier`),
  `ct`.`faction` = IF(`m`.`faction`, `m`.`faction`, `ct`.`faction`),
  `ct`.`exp` = IF(`m`.`maxlevel` AND `m`.`maxlevel` <= 62, 0, `ct`.`exp`);
UPDATE `tmp_era_zombie_ct` `ct` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `ct`.`entry` SET `ct`.`entry` = `m`.`dst`;
INSERT INTO `creature_template` SELECT * FROM `tmp_era_zombie_ct`;
DROP TEMPORARY TABLE `tmp_era_zombie_ct`;

-- The rows that hang off creature_template: models, auras, weapons, resistances, spells, movement.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_template_model` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_model` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_template_addon` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`entry`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`entry` SET `x`.`entry` = `m`.`dst`;
INSERT INTO `creature_template_addon` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_equip_template` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_equip_template` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_template_resistance` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_resistance` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_template_spell` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_spell` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_x`;
CREATE TEMPORARY TABLE `tmp_era_zombie_x` SELECT `x`.* FROM `creature_template_movement` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_zombie_x` `x` JOIN `tmp_era_zombie_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_movement` SELECT * FROM `tmp_era_zombie_x`;
DROP TEMPORARY TABLE `tmp_era_zombie_x`;

DROP TEMPORARY TABLE `tmp_era_zombie_map`;

-- The crate: a plain decoration in the original (type 5), a goober here so it can be clicked.
DELETE FROM `gameobject_template` WHERE `entry` = 9500854;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_zombie_go`;
CREATE TEMPORARY TABLE `tmp_era_zombie_go` SELECT * FROM `gameobject_template` WHERE `entry` = 190095;
UPDATE `tmp_era_zombie_go` SET
  `type` = 10,
  `Data0` = 0, `Data1` = 0, `Data2` = 0, `Data3` = 0, `Data4` = 0, `Data5` = 0, `Data6` = 0, `Data7` = 0,
  `Data8` = 0, `Data9` = 0, `Data10` = 0, `Data11` = 0, `Data12` = 0, `Data13` = 0, `Data14` = 0, `Data15` = 0,
  `AIName` = '',
  `ScriptName` = 'go_era_plagued_crate',
  `entry` = 9500854;
INSERT INTO `gameobject_template` SELECT * FROM `tmp_era_zombie_go`;
DROP TEMPORARY TABLE `tmp_era_zombie_go`;

DELETE FROM `spell_script_names` WHERE `spell_id` = 56560 AND `ScriptName` = 'spell_era_beckoning_groan';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (56560, 'spell_era_beckoning_groan');
