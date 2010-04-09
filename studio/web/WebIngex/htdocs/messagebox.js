/*
 * Copyright (C) 2009  British Broadcasting Corporation
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
 
 /*
  * General purpose message box, which overlays over the screen content
  */
 
 
// show a small 'waiting' messagebox with loading ticker 
function show_wait_messagebox(title, message){
	if($('ie6iframe')){		// running on ie 6
		$('ie6message_overlay').innerHTML = "	<div id=\"ie6message\" class=\"ie6message\">" +
	 										"		<div id=\"content\" class=\"content\">" +
	 										"			<h4><center>" + title + "</center></h4>" +
	 										"			<br>" +
	 										"			<div id=\"text_scroll\"  class=\"text_scroll\">" + message +
	 										"				<div id=\"load_indicator\" class=\"load_indicator\"></div>" +
	 										"			</div>" +
	 										"		</div>" +
	 										"		<right><div class=\"links\"><a class=\"close\" href='javascript:hide_messagebox()' onMouseOver=\"this.style.cursor='pointer'\"> close</a></div></right>";
	 	$('ie6iframe').style.visibility = 'visible';
	 	$('ie6message_overlay').style.visibility = 'visible';								
	}
	else{
		$('message_overlay').innerHTML =	"<div id=\"background\"></div>" +
											"	<div id=\"message\" class=\"message\">" +
		 									"		<div id=\"content\" class=\"content\">" +
		 									"			<h4><center>" + title + "</center></h4>" +
		 									"			<br>" +
		 									"			<div id=\"text_scroll\"  class=\"text_scroll\">" + message +
		 									"				<div id=\"load_indicator\" class=\"load_indicator\"></div>" +
		 									"			</div>" +
		 									"		</div>" +
		 									"		<right><div class=\"links\"><a class=\"close\" href='javascript:hide_messagebox()' onMouseOver=\"this.style.cursor='pointer'\"> close</a></div></right>" +
		 									"	</div>";
		$('message_overlay').style.visibility = 'visible';
	 	$('message').style.visibility = 'visible';	
	}
}
 
 
// show a fixed messagebox with scrollable message
function show_static_messagebox(title, message){
	if($('ie6iframe')){		// running on ie 6
		$('ie6message_overlay').innerHTML = 	"	<div id=\"ie6message\" class=\"ie6message\">" +
												"		<div id=\"content\" class=\"content\">" +
												"			<h4><center>" + title + "</center></h4>" +
												"			<br><div id=\"aaf_filename\"  class=\"text_scroll\">" + message + "</div>" +
												"		</div>" +
												"		<right><div class=\"links\"><a class=\"close\" href='javascript:hide_messagebox()' onMouseOver=\"this.style.cursor='pointer'\"> close</a></div></right>";
	 	$('ie6iframe').style.visibility = 'visible';	
	 	$('ie6message_overlay').style.visibility = 'visible';							
	}
	else{
		$('message_overlay').innerHTML = 	"<div id=\"background\"></div>" +
											"	<div id=\"message\" class=\"message\">" +
											"		<div id=\"content\" class=\"content\">" +
											"			<h4><center>" + title + "</center></h4>" +
											"			<br><div id=\"aaf_filename\"  class=\"text_scroll\">" + message + "</div>" +
											"		</div>" +
											"		<right><div class=\"links\"><a class=\"close\" href='javascript:hide_messagebox()' onMouseOver=\"this.style.cursor='pointer'\"> close</a></div></right>" +
											"	</div>";
		$('message_overlay').style.visibility = 'visible';
	 	$('message').style.visibility = 'visible';	
	}
}
 
 
// hide the messagebox
function hide_messagebox(){
	if($('ie6iframe')){		// running on ie 6
		$('ie6iframe').style.visibility = 'hidden';
		$('ie6message_overlay').style.visibility = 'hidden';
	}
 	else{ 
 		$('message_overlay').style.visibility = 'hidden';
 		$('message').style.visibility = 'hidden';
 	}
}