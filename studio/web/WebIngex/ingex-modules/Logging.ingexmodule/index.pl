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
                            
                            <!-- COMMENTED OUT <td>
                            Producer:
                            <br /><input type='text' id='producer' name='producer' class='progInfoFormElement' value='' disabled=true>
                         <div class='spacer'>&nbsp;</div>
                            Director:
                            <br /><input type='text' id='director' name='director' class='progInfoFormElement' value='' disabled=true>
                         </td><td>
                            PA:
                            <br /><input type='text' id='pa' name='pa' class='progInfoFormElement' value='' disabled=true>
                        </td> -->
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
                <a class='simpleButton' href='javascript:ILloadNewProgInfo();'>Reset</a>
                <a class='simpleButton' href='javascript:ILexpandItems();'>Expand All</a>
                <a class='simpleButton' href='javascript:ILcollapseItems();'>Collapse All</a>
                
            </td><td>   
                <a class='simpleButton' href='javascript:ILformCall(\"new\");'>New Item</a>
                <a class='simpleButton' href='javascript:ILformCall(\"pickup\");'>Pickup</a>
                <a class='simpleButton' href='javascript:ILformCall(\"edit\");'>Edit</a>
            </td><td>
                <a class='simpleButton' href='javascript:ILdeleteItem();'>Delete Item</a>
            </td><td>   
                <a class='simpleButton' href='javascript:ILimport();'>Import</a>
                <a class='simpleButton' href='javascript:ILprint();'>Print</a>
                <a class='simpleButton' href='javascript:ILpdf();'>PDF</a>  
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
                              <div class='itemName'>
                                    <span id='currentItemName' class='itemNameNone' >No Item Selected</span>
                                </div>
                      </td>
                      <td width='200px' vertical-align='middle'>
                            Next Take: <div id='currentTakeNum' class='itemNameNone'>No Takes</div>
                       </td>
                      </tr>
                      <tr>
                      <td width='700px'>
                                 <div class='leftButtons'>
                                    <a class='simpleButton' href='javascript:ILselectPrevItem();'>prev</a>
                                    <a class='simpleButton' href='javascript:ILselectNextItem();'>next</a>
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
                                    <select name='recordingLocation' class='takeInfoFormElement' id='recLocSelector'>
                                           <option value='0'>Loading...</option>
                                   </select>
                                </td>
                                </tr>
                                </tbody>
                            </table>
                        </fieldset>
                        <div class='spacer'>&nbsp;</div>
                        <fieldset>
                        <table class='alignmentTable'>
                            <tbody>
                            <tr>
                                <th>Result:</th>
                            </tr>
                            <tr>
                                <td id='resultText' class='good'>Good</td>
                            </tr>
                            <tr>
                            <td>
                                <div class ='centerButtons'>
                                       <a class='simpleButton' href='javascript:ILsetGood();'>Good</a>
                                       <a class='simpleButton' href='javascript:ILsetNoGood();'>No Good</a>
                            </div>
                            </td>
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
                                    <th>In:</th>
                                    <td><input type='text' name='in' class='takeDetailsFormElement' value='00:00:00:00' id='inpointBox' disabled='disabled'></td>
                                </tr>    
                                <tr>   
                                    <th>Out:</th>
                                    <td><input type='text' name='out' class='takeDetailsFormElement' value='00:00:00:00' id='outpointBox' disabled='disabled'></td>
                                </tr>   
                                <tr>    
                                    <th>Duration:</th>
                                    <td><input type='text' name='duration' class='takeDetailsFormElement' value='00:00:00:00' id='durationBox' disabled='disabled'></td>
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
                            <div class='rightButtons'>
                             <table class ='alignmentTable'>
                                <tbody>
                                <tr>
                                    <td>
                                     <a class='startButton' href='javascript:ILstartStop();' id='startStopButton'>Start</a>
                                     <a class='simpleButton' href='javascript:ILresetTake();'>Clear Details</a>
                                     <a class='simpleButton' href='javascript:ILstoreTake();'>Store Take</a>
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


