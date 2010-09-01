-- Test local infax data
--
-- Note: src_identifier must < 128
--


INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   1, 1, 'DA 999999');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 0', 'TEST 0',
   '2007-12-01', 'A', 'XYZ1234A',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 0', 'ONLY USE FOR TESTING', 45000,
   'DA 999999', 'DA999999', NULL,
   1, '12F12', 1);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   2, 1, 'DA 999998');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 1', 'TEST 1',
   '2007-12-01', 'A', 'XYZ1234B',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 1', 'ONLY USE FOR TESTING', 45000,
   'DA 999998', 'DA999998', NULL,
   1, '16L12', 2);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   3, 1, 'DA 999997');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 2', 'TEST 2',
   '2007-12-01', 'A', 'XYZ1234C',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 2', 'ONLY USE FOR TESTING', 45000,
   'DA 999997', 'DA999997', NULL,
   1, NULL, 3);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   4, 1, 'DA 999996');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 3', 'TEST 3',
   '2007-12-01', 'A', 'XYZ1234D',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 3', 'ONLY USE FOR TESTING', 45000,
   'DA 999996', 'DA999996', NULL,
   1, '12F12', 4);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   5, 1, 'DA 999995');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 1',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   1, '12F12', 5);
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 2',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   2, '12F12', 5);
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 3',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   3, '12F12', 5);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   6, 1, 'BBD999999');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'BD', 'TEST 0', 'TEST 0',
   '2009-12-01', 'A', 'XYZ1234A',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 0', 'ONLY USE FOR TESTING', 45000,
   'BBD999999', 'CU999999', NULL,
   1, '16F16', 6);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   7, 1, 'BBD999998');
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'BD', 'TEST 1', 'TEST 1 - item 1',
   '2009-12-01', 'A', 'XYZ1234A',
   '0', 'M', '2008-11-14',
   'TEST SPOOL 1', 'ONLY USE FOR TESTING', 25000,
   'BBD999998', 'CU999998', NULL,
   1, '16F16', 7);
INSERT INTO SourceItem (
   sit_format, sit_prog_title,  sit_episode_title,
   sit_tx_date, sit_mag_prefix, sit_prog_no,
   sit_prod_code, sit_spool_status, sit_stock_date,
   sit_spool_descr, sit_memo, sit_duration,
   sit_spool_no, sit_acc_no, sit_cat_detail,
   sit_item_no, sit_aspect_ratio_code, sit_source_id)
VALUES (
   'BD', 'TEST 1', 'TEST 1 - item 2',
   '2009-12-01', 'A', 'XYZ1234A',
   '0', 'M', '2008-11-14',
   'TEST SPOOL 1', 'ONLY USE FOR TESTING', 25000,
   'BBD999998', 'CU999998', NULL,
   2, '16L12', 7);

   
SELECT setval('src_id_seq', 100);
SELECT setval('sit_id_seq', 100);

