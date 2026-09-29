-- mod-era-events: Gates of Ahn'Qiraj creatures (tier 4).
--
-- Copies of the Blizzard creatures at 9500810-9500818 with this module's scripts. The originals run
-- the quest flashback A Pawn on the Eternal Board (npc_qiraj_war_spawn, npc_anachronos_*). Health is
-- brought down so a handful of players and their bots can hold the line. Entries must match
-- src/events/AQWar.cpp.
--
-- Written as copies through temporary tables so no column names of creature_template appear here;
-- the server's schema differs from AzerothCore master (creature.id vs id1).
--
-- Idempotent: safe to run again.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_template_resistance` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_template_movement` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;
DELETE FROM `creature_text` WHERE `CreatureID` BETWEEN 9500810 AND 9500818;

DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_map`;
CREATE TEMPORARY TABLE `tmp_era_aq_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL, `script` VARCHAR(64) NOT NULL, `minlevel` TINYINT UNSIGNED NOT NULL, `maxlevel` TINYINT UNSIGNED NOT NULL, `health` FLOAT NOT NULL, `faction` SMALLINT UNSIGNED NOT NULL);
INSERT INTO `tmp_era_aq_map` VALUES
(15454, 9500810, 'npc_era_anchor', 0, 0, 0, 0), -- Anachronos Quest Trigger Invisible: the event anchor
(15423, 9500811, 'npc_era_event_mob', 60, 60, 3, 0), -- Kaldorei Infantry: friendly to every player, hostile to the swarm
(15414, 9500812, 'npc_era_event_mob', 60, 60, 1.5, 0), -- Qiraji Wasp
(15422, 9500813, 'npc_era_event_mob', 60, 60, 2, 0), -- Qiraji Tank
(15424, 9500814, 'npc_era_event_mob', 61, 61, 4, 0), -- Anubisath Conqueror
(15378, 9500815, 'npc_era_aq_dragon', 0, 0, 0, 35), -- Merithra of the Dream
(15380, 9500816, 'npc_era_aq_dragon', 0, 0, 0, 35), -- Arygos
(15379, 9500817, 'npc_era_aq_dragon', 0, 0, 0, 35), -- Caelestrasz
(15818, 9500818, 'npc_era_event_mob', 62, 62, 25, 0); -- Lieutenant General Nokhor: the final wave

-- creature_template: copy, give it our script, drop SmartAI, set the era's level. The
-- entry changes in its own statement: a multi-table UPDATE doesn't order its assignments.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_ct`;
CREATE TEMPORARY TABLE `tmp_era_aq_ct` SELECT `ct`.* FROM `creature_template` `ct` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `ct`.`entry`;
UPDATE `tmp_era_aq_ct` `ct` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `ct`.`entry` SET
  `ct`.`AIName` = '',
  `ct`.`ScriptName` = `m`.`script`,
  `ct`.`minlevel` = IF(`m`.`minlevel`, `m`.`minlevel`, `ct`.`minlevel`),
  `ct`.`maxlevel` = IF(`m`.`maxlevel`, `m`.`maxlevel`, `ct`.`maxlevel`),
  `ct`.`HealthModifier` = IF(`m`.`health`, `m`.`health`, `ct`.`HealthModifier`),
  `ct`.`faction` = IF(`m`.`faction`, `m`.`faction`, `ct`.`faction`),
  `ct`.`exp` = IF(`m`.`maxlevel` AND `m`.`maxlevel` <= 62, 0, `ct`.`exp`);
UPDATE `tmp_era_aq_ct` `ct` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `ct`.`entry` SET `ct`.`entry` = `m`.`dst`;
INSERT INTO `creature_template` SELECT * FROM `tmp_era_aq_ct`;
DROP TEMPORARY TABLE `tmp_era_aq_ct`;

-- The rows that hang off creature_template: models, auras, weapons, resistances, spells, movement.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_template_model` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_model` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_template_addon` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`entry`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`entry` SET `x`.`entry` = `m`.`dst`;
INSERT INTO `creature_template_addon` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_equip_template` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_equip_template` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_template_resistance` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_resistance` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_template_spell` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_spell` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.* FROM `creature_template_movement` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` `x` JOIN `tmp_era_aq_map` `m` ON `m`.`src` = `x`.`CreatureID` SET `x`.`CreatureID` = `m`.`dst`;
INSERT INTO `creature_template_movement` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;

-- Voice lines. Range 0 (yell distance) instead of zone-wide: yells respect phases, zone
-- messages would reach players of every era.
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_text_map`;
CREATE TEMPORARY TABLE `tmp_era_aq_text_map` (`src` INT UNSIGNED NOT NULL, `dst` INT UNSIGNED NOT NULL);
INSERT INTO `tmp_era_aq_text_map` VALUES (15378, 9500815), (15380, 9500816), (15379, 9500817);
DROP TEMPORARY TABLE IF EXISTS `tmp_era_aq_x`;
CREATE TEMPORARY TABLE `tmp_era_aq_x` SELECT `x`.*, `m`.`dst` AS `era_dst` FROM `creature_text` `x` JOIN `tmp_era_aq_text_map` `m` ON `m`.`src` = `x`.`CreatureID`;
UPDATE `tmp_era_aq_x` SET `CreatureID` = `era_dst`, `TextRange` = 0;
ALTER TABLE `tmp_era_aq_x` DROP COLUMN `era_dst`;
INSERT INTO `creature_text` SELECT * FROM `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_x`;
DROP TEMPORARY TABLE `tmp_era_aq_text_map`;

DROP TEMPORARY TABLE `tmp_era_aq_map`;

-- The dragons keep their quest-giver flags from the original; nothing to talk to here.
UPDATE `creature_template` SET `npcflag` = 0 WHERE `entry` BETWEEN 9500815 AND 9500817;
