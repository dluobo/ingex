// Copyright (C) 2009  British Broadcasting Corporation
// Author: Sean Casey <seantc@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Database.h>
#include <DBException.h>
#include <DatabaseEnums.h>

using namespace std;
using namespace prodauto;

int main(int argc, char* argv[])
{
	char *inStr = argv[1];

	// Delete packages from database
	try
	{
		// Initialise
		prodauto::Database::initialise("localhost", "prodautodb", "bamzooki", "bamzooki", 4, 12);

		Database* db = Database::getInstance();
		Transaction* transaction = (db->getTransaction("DeletePackages"));

		char *nextTok = strtok(inStr, ",");
		if (!nextTok) {
			printf("Error! No package IDs supplied.\n");
			return 1;
		}

		size_t total_packages = 0;
        vector<long> packages;
		while (nextTok){
			packages.push_back(atol(nextTok));
			total_packages++;

			// limit number of packages to limit memory usage
			if (packages.size() > 500) {
			    db->deleteMaterialPackageChains(packages, transaction);
			    packages.clear();
			}

			nextTok = strtok(NULL, ",");
		}
        if (!packages.empty())
            db->deleteMaterialPackageChains(packages, transaction);


        transaction->commit();

		printf("Deleted %zu material item(s)\n", total_packages);
		return 0;

		//TODO: Check for unreferenced isolated packages
	}
	catch (const prodauto::DBException & dbe)
	{
		printf("Error\n");
		return 1;
	}
}

