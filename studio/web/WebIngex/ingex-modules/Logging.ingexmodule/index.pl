#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
 
use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use lib "../../ingex-config";
use ingexhtmlutil;



my $page = get_page_content() 
    or return_error_page("Failed to fill in content for this page.");
   
print header;
print $page;

exit(0);



sub get_page_content
{
    return "
	<h1> Production Logging</h1>
	
	<div id='logging'>
		<div id='programmeInfo'>		
			<h1>Programme</h1>
				<table class=alignmentTable><tr><td>
					<table class=alignmentTable><tr><td>
							Series: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::series\");ingexAddToHistory(\"series\");'>edit</a>)</span>
							<br />
							<select name='series' class='progInfoFormElement' id='seriesSelector' onChange='ILupdateProgrammes()'>
								<option value='0'>Loading...</option>
							</select>
						</td><td>
						
							Programme: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::prog\");ingexAddToHistory(\"prog\");'>edit</a>)</span>
							<br />
							<select name='series' class='progInfoFormElement' id='programmeSelector' onChange='ILloadNewProg()'>
							</select>
						</td><td>	
							Producer:
							<br /><input type='text' id='producer' name='producer' class='progInfoFormElement' value=''>
						</td><td>
							Director:
							<br /><input type='text' id='director' name='director' class='progInfoFormElement' value=''>
						</td><td>
							PA:
							<br /><input type='text' id='pa' name='pa' class='progInfoFormElement' value=''>
						</td>
					</tr>
					</table>
				</tr>
				</table>
		</div>	
	
		<div id='takes'>
			<h1>Recorded Takes</h1>
			<div id='takesList'></div>
			<div class='spacer'>&nbsp;</div>
			<table class=alignmentTable><tr><td>
				
			<div class='rightButtons'>
				<a class='simpleButton' href='javascript:ILloadNewProg();'>Refresh Data</a>
				<a class='simpleButton' href='javascript:ILduplicateNode();'>Duplicate Item</a>
				<a class='simpleButton' href='javascript:ILdeleteNode();'>Delete Item</a>
			</div>

			<div id= 'newItem'>
				<h1>New Item</h1>
				<div class ='LeftButtons'>
				<a class='simpleButton' href='javascript:ILstoreItem();'>New Item</a>
				</div>	
					
					<table class='alignmentTable'>
						<tr>
							<th>Item Info</th>
							<td><input type='text' name='itemName' class='itemInfoFormElement' value='' id='itemBox'></td>
							
						</tr>
						<tr>
							<th>Sequence Info</th>
							<td><input type='text' name='seq' class='itemInfoFormElement' value='' id='seqBox'></td>
							
						</tr>
					</table>
			</div>	
			</td><td>
			<div id='actions'>
			<h1>Actions</h1>
			<p class='center'>
				<a class='simpleButton' href='javascript:ILimport();'>Import</a>
				<a class='simpleButton' href='javascript:ILprint();'>Print</a>
			</p>
			</div>
			</td><td>
				<div id='recorderSelect'>
				<h1>Recorder</h1>
				<div class='spacer'>&nbsp;</div>
					<select name='recorder' class='recInfoFormElement' id='recorderSelector' onChange='ILtc.incrementMethod()'>
						<option value='0'>Loading...</option>
					</select>
					
			</div>
			<div class='spacer'>&nbsp;</div>
			<div id='currentTC'>
				<h1>Timecode</h1>
				<p class='center' id='tcDisplay'>--:--:--:--</p>
			</div>

			</td></tr></table>
				
		</div>
			<div id='newTake'>	
				<h1>New Take</h1>
				<table class='alignmentTable'>
				<tr>
				<td>
					<div id='takeInfo'>
					<fieldset>
					<legend>
					Take Info
					</legend>
				
					<table class='alignmentTable'>
						<div class='spacer'>&nbsp;</div>
						<tr>
							<th>Recording Location:</th>
						</tr>	
						<tr>
							<td>
								<select name='recordingLocation' class='takeInfoFormElement' id='recLocSelector'>
									<option value='studioA'>Loading...</option>
								</select>
							</td>
						</tr>	<div class='spacer'>&nbsp;</div>
						<tr>
							<th>Item/Shot:</th>
						</tr>
						<tr>
							<td>
								<select name='itemSelection' class='takeInfoFormElement' id='itemSelector'>
									<option value='0'></option>
								</select>
								<div class='rightButtons'>
									<a class='small simpleButton' href='javascript:ILselectPrevItem();'>prev</a>
									<a class='small simpleButton' href='javascript:ILselectNextItem();'>next</a>
								</div>
							</td>
						</tr>
					</table>
					</fieldset>
					</div>
				</td>
				<td>
					<div id=takeDetails'>
					<fieldset>
					<legend>
					Take Details
					</legend>
					<table class='alignmentTable'>
					<tr>
					<td>
						<table class='alignmentTable'>
						<tr>
							<th>In:</th>
							<td><input type='text' name='in' class='takeDetailsFormElement' value='00:00:00:00' id='inpointBox'></td>
						</tr>	<div class='spacer'>&nbsp;</div>	
						<tr>	
							<th>Out:</th>
							<td><input type='text' name='out' class='takeDetailsFormElement' value='00:00:00:00' id='outpointBox'></td>
						</tr> 	<div class='spacer'>&nbsp;</div>	
						<tr>	
							<th>Duration:</th>
							<td><input type='text' name='duration' class='takeDetailsFormElement' value='00:00:00:00' id='durationBox'></td>
						
						</tr>
						</table>
					</td>
					<td>
						<table class ='alignmentTable'>
						<tr>
							<th>Result:</th>
							<td id='resultText' class='noGood'>No Good</td>
						</tr>
						<tr>
							<th style='width:50px'>Comments:</th>
							<td ><textarea id='commentBox'></textarea></td>
						</tr>
						</table>
					</td>
					</tr></table>
					<div class='spacer'>&nbsp;</div>
					</fieldset>
					</div>
				</td>
				</tr>
				</table>
				<div class='spacer'>&nbsp;</div>
				<div class='leftButtons'>
					<a class='simpleButton' href='javascript:ILstartStop();' id='startStopButton'>Start</a>
				</div>
				<div class='leftExtraButtons'>
					<a class='simpleButton' href='javascript:ILsetGood();'>Good</a>
					<a class='simpleButton' href='javascript:ILsetNoGood();'>No Good</a>
				</div>
				<div class='rightButtons'>
					<a class='simpleButton' href='javascript:ILresetTake();'>Cancel</a>
					<a class='simpleButton' href='javascript:ILstoreTake();'>Store Take</a>
				</div>
			</div>

	</div>	
	
	<div class='displayOptions'><h5>Display Options:</h5> <a href='javascript:toggleTC()'>Toggle timecode frame display</a></div>
	";
}


