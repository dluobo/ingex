#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
# Modified 2011
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
use Encode;

use lib ".";
use lib "../../ingex-config";
use ingexhtmlutil;



my $page = get_page_content() 
    or return_error_page("Failed to fill in content for this page.");
   
print header('text/html; charset=utf-8');
print encode_utf8($page);

exit(0);



sub get_page_content
{
    return "
	<div id='logging'>
        <div id='programmeInfo'>        
            <table class='alignmentTable'>
            <tbody>
            <tr>
                    <td>
                        Series: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::series\");ingexAddToHistory(\"series\");'>edit</a>)</span>
                            <br />
                            <select name='series' class='progInfoFormElement' id='seriesSelector' onchange='ILupdateProgrammes()'>
                                <option value='0'>Loading...</option>
                            </select>
                            </td><td>
                            Programme: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::prog\");ingexAddToHistory(\"prog\");'>edit</a>)</span>
                            <br />
                            <select name='series' class='progInfoFormElement' id='programmeSelector' onchange='ILloadNewProgInfo()'>
                            </select>
                    </td><td>
                          Recorder:
                          <br />
                           <select name='recorder' class='recInfoFormElement' id='recorderSelector' onChange='ILtc.incrementMethod()'>
                              <option value='0'>Loading...</option>
                           </select>
                           </td><td>
                          <div class='spacer'>&nbsp;</div>
                            Timecode:
                            <br />  
                            <p class='tcNum' id='tcDisplay'>--:--:--:--</p>
                           </td>
                </tr>
                </tbody>
            </table>
        </div>
            
        <div id='itemForm' class = 'x-hidden'></div>
        <div id='takeForm' class = 'x-hidden'></div>
        
        <div id='takes'>
            <h1>Items and Recorded Takes</h1>
            <div id='takesList'></div>
            <div class='spacer'>&nbsp;</div>
           
          <table class='alignmentTable'>
          <tbody>
          <tr>
          <td>
                <form><input type='button' class='buttons' id='resetButton' value='Reset' onclick='ILloadNewProgInfo()' /> <input type='button' class='buttons' id='expandAllButton' value='Expand All' onclick='ILexpandItems()' /> <input type='button' class='buttons' id='collapseAllButton' value='Collapse All' onclick='ILcollapseItems()' /></form>
            </td><td>
                <form> <input type='button' class='buttons' id='newItemButton' value='New Item' onclick='ILformCall(\"new\")' /> <input type='button' class='buttons' id='pickupItemButton' value='Pickup' onclick='ILformCall(\"pickup\")' /> <input type='button' class='buttons' id='editItemButton' value='Edit' onclick='ILformCall(\"edit\")' /></form>
            </td><td>
                <form><input type='button' class='buttons' id='deleteItemButton' value='Delete Item' onclick='ILdeleteItem()' /></form>
               <!-- <a class='simpleButton' href='javascript:ILdeleteItem();'>Delete Item</a> -->
            </td><td>
            <form><input type='button' class='buttons' id='importButton' value='Import' onclick='ILimport()' /> <input type='button' class='buttons' id='printButton' value='Print' onclick='ILprint()' /> <input type='button' class='buttons' id='pdfButton' value='PDF' onclick='ILpdf()' /></form>
            </td>
            </tr>
            </tbody>
            </table>
            <div class='spacer'>&nbsp;</div>
         </div>
         <div id='newTake'>
         <h1>New Take</h1>
         <table class='alignmentTable'>
         <tbody>
         <tr>
         <td>
            <fieldset>
            <legend>
            Currently Selected Item/Shot:
            </legend>
             <table class='alignmentTable'>
                <tbody>
                    <tr>
                      <td>
                              <div>
                                    <span id='currentItemName' class='itemNameNone' >No Item Selected</span>
                              </div>
                      </td>
                      <td width='100px'>
                                <div>
                                    <form><input type='button' class='buttons' id='prevItemButton' value='prev' onclick='ILselectPrevItem()' /> &nbsp <input type='button' class='buttons' id='nextItemButton' value='next' onclick='ILselectNextItem()' /></form>
                                </div>
                      </td>
                     </tr>
                  </tbody>
              </table>
             </fieldset>
            </td>
         </tr>
         <tr>
           <td>
           <div id='takeDetails'>
               <fieldset>
               <legend>
                Take Details
               </legend>
                    <table class='alignmentTable'>
                    <tbody>
                    <tr>
                       <td>
                       <fieldset>
                           <table class='alignmentTable'>
                                <tbody>
                                <tr> 
                                   <th>Recording Location:</th>
                                 </tr>
                                 <tr>
                                 <td>    
                                    <select name='recordingLocation' class='takeInfoFormElement' id='recLocSelector' onchange='javascript:this.blur();'>
                                           <option value='0'>Loading...</option>
                                   </select>
                                </td>
                                </tr>
                                <tr>
                                 <td width='200px' vertical-align='middle'>
                                            Next Take: <div id='currentTakeNum' class='itemNameNone'>-</div>
                                 </td>
                                 </tr>
                                 <tr>
                                 <td>
                                        <form><input type='button' class='startButton' id='startButton' value='Start' onclick='ILstartStop()' /> &nbsp <input type='button' class='stopButton' id='stopButton' value='Stop' onclick='ILstartStop()' /></form>
                                 </td>
                                 
                                </tr>
                                <tr>
                                    
                                </tr>
                                </tbody>
                            </table>
                        </fieldset> 
                        </td>
                        <td width='500px'>
                        <fieldset>
                          <table class='alignmentTable'>
                          <tbody>
                            <tr>
                            <th>Comments:</th>
                            </tr>
                            <tr>
                                <td><textarea id='commentBox'></textarea></td>
                            </tr>
                          </tbody>
                          </table>
                        </fieldset>
                        </td>
                        <td>
                            <table class='alignmentTable'>
                            <tbody>
                                <tr>
                                    Circled Status:
                                </tr>
                                <tr>
                                   <td>
                                         <table class='alignmentTable'>
                                            <tbody>
                                            <tr>
                                               <td id='resultText' class='notCircled'>Not Circled</td>
                                            </tr>
                                            <tr>
                                            <td>
                                                <div>
                                                    <form style='float:right'><input type='button' class='buttons' id='setCircledButton' value='Circled' onclick='ILsetCircled()' /> <input type='button' class='buttons' id='setNotCircledButton' value='Not Circled' onclick='ILsetNotCircled()' /></form>
                                            </div>
                                            </td>
                                            </tr>
                                            </tbody>
                                        </table>
                                    </td>
                                </tr>
                             </tbody>
                             </table>
                             <table class='alignmentTable'>
                             <tbody>
                                <tr>
                                    <th>In:</th>
                                    <td> <p class=''takeDetailsFormElement' id='inpointBox' name='in'>00:00:00:00</p></td>
                                </tr>    
                                <tr>   
                                    <th>Out:</th>
                                    <td><p class=''takeDetailsFormElement' id='outpointBox' name='out'>00:00:00:00</p></td>
                                </tr>   
                                <tr>    
                                    <th>Duration:</th>
                                    <td><p class=''takeDetailsFormElement' id='durationBox' name='duration'>00:00:00:00</p></td>
                                </tr>
                            </tbody>
                            </table>
                        </td>
                    </tr>
                    </tbody>
                    </table>
                    <div class='spacer'>&nbsp;</div>
                    <table class='alignmentTable'>
                    <tbody>
                    <tr>
                       <td>
                             <div>
                             <table class ='alignmentTable'>
                                <tbody>
                                <tr>
                                    <td>
                                     <form style='float:right'><input type='button' class='buttons' id='resetTakeButton' value='Clear Details' onclick='ILresetTake()' /> <input type='button' class='buttons' id='storeTakeButton' value='StoreTake' onclick='ILstoreTake()' /></form>
                                     </td>
                                 </tr>
                                 </tbody>
                            </table>
                            </div>
                        </td>
                    </tr>
                    </tbody>
                    </table>
                    <div class='spacer'>&nbsp;</div>
                    </fieldset>
                    </div>
            </td>
            </tr>
            </tbody>
            </table>
            </div>
        </div>           
    
    
    <div class='displayOptions'><h5>Display Options:</h5> <a href='javascript:toggleTC()'>Toggle timecode frame display</a></div>
  ";
}


