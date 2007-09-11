BEGIN;



INSERT INTO Series
    (srs_identifier, srs_name)
VALUES
    (1, 'BAMZOOKi');
INSERT INTO Series
    (srs_identifier, srs_name)
VALUES
    (2, 'EastEnders');
INSERT INTO Series
    (srs_identifier, srs_name)
VALUES
    (3, 'Demo');
SELECT setval('srs_id_seq', max(srs_identifier)) FROM Series;

    
-- PROGRAMME

INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (1, 'Grand Final', 1);
INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (2, 'Episode 100', 2);
INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (3, 'Episode 101', 2);
INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (4, 'Test 1', 3);
INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (5, 'Test 2', 3);
INSERT INTO Programme
    (prg_identifier, prg_name, prg_series_id)
VALUES
    (6, 'Test 3', 3);
SELECT setval('prg_id_seq', max(prg_identifier)) FROM Programme;


-- ITEM

INSERT INTO Item
    (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
VALUES
    (1, 'Jake introduces the contestants', ARRAY['1', '2'], 256, 1);

INSERT INTO Item
    (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
VALUES
    (2, 'The zooks fight it out', ARRAY['3'], 512, 1);

INSERT INTO Item
    (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
VALUES
    (3, 'The winner is announced', ARRAY['4', '5', '6'], 768, 1);

-- ITEMS FOR PROGRAMME "Test 1"

INSERT INTO Item
    (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
VALUES
    (4, 'Testing', ARRAY['1'], 256, 4);

INSERT INTO Item
    (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
VALUES
    (5, 'More testing', ARRAY['2', '3'], 512, 4);

-- END OF ITEMS
SELECT setval('itm_id_seq', max(itm_identifier)) FROM Item;


-- TAKE

INSERT INTO Take
    (tke_identifier, tke_number, tke_comment, tke_result, tke_edit_rate, tke_length, tke_start_position, tke_start_date, tke_recording_location, tke_item_id)
VALUES
    (1, 1, 'Forgot line', 3, (25, 1), 100, 90000, '2006-10-30', 6, 1);
    
INSERT INTO Take
    (tke_identifier, tke_number, tke_result, tke_edit_rate, tke_length, tke_start_position, tke_start_date, tke_recording_location, tke_item_id)
VALUES
    (2, 2, 2, (25, 1), 200, 91000, '2006-10-30', 6, 1);
    
INSERT INTO Take
    (tke_identifier, tke_number, tke_comment, tke_result, tke_edit_rate, tke_length, tke_start_position, tke_start_date, tke_recording_location, tke_item_id)
VALUES
    (3, 1, '', 2, (25, 1), 300, 92000, '2006-10-30', 6, 2);
    
INSERT INTO Take
    (tke_identifier, tke_number, tke_comment, tke_result, tke_edit_rate, tke_length, tke_start_position, tke_start_date, tke_recording_location, tke_item_id)
VALUES
    (4, 1, 'Tripped badly', 3, (25, 1), 5000, 95000, '2006-10-30', 6, 3);
    
INSERT INTO Take
    (tke_identifier, tke_number, tke_result, tke_edit_rate, tke_length, tke_start_position, tke_start_date, tke_recording_location, tke_item_id)
VALUES
    (5, 2, 2, (25, 1), 500, 100000, '2006-10-30', 6, 3);
    
    
SELECT setval('tke_id_seq', max(tke_identifier)) FROM Take;



END;

