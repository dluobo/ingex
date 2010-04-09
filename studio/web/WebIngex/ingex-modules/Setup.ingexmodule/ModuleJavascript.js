/*
 * Copyright (C) 2010  British Broadcasting Corporation
 * Author: Sean Casey <seantc@users.sourceforge.net>
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

// --- Template Module Javascript ---

// It is HIGHLY recommended that you prefix any variable or
// function names with your module name. e.g. Template_myFunction()
//
//var tipsData = 	{
//					'index':{
//						'genCallout':'Identifiers for physical recording locations',
//						'projCallout':'Names of current productions',
//						'seriesCallout':'Names of current series',
//						'progCallout':'Names of current programmes',
//						'sourceCallout':'Camera/audio source definitions',
//						'recCallout':'Setup Ingex recorders and encoder settings',
//						'multiCallout':'Configure multicam groupings',
//						'proxyCallout':'?',
//						'routerCallout':'Router setup?'
//					},
//					
//					'creategcf':{
//						'reclocNameCallout':'Enter a label for this location'
//					}
//				};
//
//// We might want to define a function which will run when a tab is loaded.
//// First, create a function to do what you want:
//var onLoad_Setup = function() {
//	init_Setup();	
//}
//// Then, add it to the LoaderFunctions object using the name of the module as the key
//loaderFunctions.Setup = onLoad_Setup;
//
//function init_Setup() {
//	setTipData('/Setup.ingexmodule/index.pl', tipsData['index']);
//	setTipData('/Setup.ingexmodule/creategcf.pl', tipsData['creategcf']);
//}