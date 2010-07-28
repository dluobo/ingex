/*
 * $Id: import_material.cpp,v 1.2 2010/07/28 17:19:41 john_f Exp $
 *
 * Import material Package Groups XML files into the database.
 *
 * Copyright (C) 2010 British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <vector>
#include <map>

#include <Database.h>
#include <PackageGroup.h>
#include <DatabaseCache.h>
#include <ProdAutoException.h>
#include <Logging.h>

using namespace std;
using namespace prodauto;


const char * const DEFAULT_DBHOST = "localhost";
const char * const DEFAULT_DBNAME = "prodautodb";
const char * const DEFAULT_DBUSER = "bamzooki";
const char * const DEFAULT_DBPW = "bamzooki";



static bool package_group_exists(PackageGroup *package_group, Transaction *transaction)
{
    Package *existing_package = Database::getInstance()->loadPackage(
            package_group->GetMaterialPackage()->uid, false, transaction);
    if (existing_package) {
        delete existing_package;
        return true;
    }

    return false;
}

static void save_package_group(PackageGroup *package_group, map<UMID, SourcePackage*> *source_packages,
                               Transaction *transaction)
{
    // check whether tape/live source package already exists to avoid the exception when trying to save it
    SourcePackage *tape_source_package = package_group->GetTapeSourcePackage();
    SourcePackage *db_existing_source_package = 0;
    if (tape_source_package) {
        Package *existing_package = 0;
        SourcePackage *existing_source_package = 0;
        map<UMID, SourcePackage*>::const_iterator search_result = source_packages->find(tape_source_package->uid);
        if (search_result == source_packages->end()) {
            existing_package = Database::getInstance()->loadPackage(tape_source_package->uid, false, transaction);
            if (existing_package) {
                db_existing_source_package = dynamic_cast<SourcePackage*>(existing_package);
                PA_CHECK(db_existing_source_package);
                source_packages->insert(make_pair(db_existing_source_package->uid, db_existing_source_package));
                existing_source_package = db_existing_source_package;
            }
        } else {
            existing_source_package = search_result->second;
        }

        if (existing_source_package)
            package_group->SetTapeSourcePackage(existing_source_package, false);
    }


    package_group->SaveToDatabase(transaction);
    transaction->commit();


    if (db_existing_source_package)
        source_packages->insert(make_pair(db_existing_source_package->uid, db_existing_source_package));
}


static void print_usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s --dbhost <host> --dbname <name> --dbuser <user> --dbpw <password> [files]\n", cmd);
}

int main(int argc, const char **argv)
{
    string dbhost = DEFAULT_DBHOST;
    string dbname = DEFAULT_DBNAME;
    string dbuser = DEFAULT_DBUSER;
    string dbpw = DEFAULT_DBPW;
    vector<const char*> file_list;
    int cmdln_index;

    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--dbhost") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbhost = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbname") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbname = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbuser") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbuser = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbpw") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbpw = argv[cmdln_index];
        }
        else
        {
            file_list.push_back(argv[cmdln_index]);
        }
    }

    Database *db;
    Database::initialise(dbhost, dbname, dbuser, dbpw, 2, 8);
    db = Database::getInstance();

    DatabaseCache db_cache;

    unsigned int error_count = 0;
    unsigned int imported_file_count = 0;
    unsigned int existing_file_count = 0;
    map<UMID, SourcePackage*> source_packages;

    size_t i;
    for (i = 0; i < file_list.size(); i++) {
        try
        {
            printf("Importing '%s':\n", file_list[i]);

            PackageGroup package_group;
            package_group.RestoreFromFile(file_list[i], &db_cache);
            
            auto_ptr<Transaction> transaction(db->getTransaction("import material"));

            if (!package_group_exists(&package_group, transaction.get())) {
                save_package_group(&package_group, &source_packages, transaction.get());
                imported_file_count++;
            } else {
                existing_file_count++;
            }
        }
        catch (const ProdAutoException &ex)
        {
            fprintf(stderr, "Error: %s\n", ex.getMessage().c_str());
            error_count++;
        }
        catch (...)
        {
            fprintf(stderr, "Error:\n");
            error_count++;
        }
    }

    printf("\nResults:\n");
    printf("%zu files processed\n", file_list.size());
    printf("%d files imported\n", imported_file_count);
    printf("%d files were already imported\n", existing_file_count);
    printf("%d files resulted in an error\n", error_count);


    map<UMID, SourcePackage*>::iterator iter;
    for (iter = source_packages.begin(); iter != source_packages.end(); iter++)
        delete iter->second;


    int return_value = 0;
    if (file_list.size() == error_count)
        return_value = 1;


    return return_value;
}

