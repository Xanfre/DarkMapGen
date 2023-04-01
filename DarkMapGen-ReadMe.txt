DarkMapGen
==========

A tool to define and generate auto-map location sub-images from map images used by the Dark engine, in Thief 1,
Thief 2 (v1.19+) and System Shock 2 (v2.4+).

The tool only works with 24-/32-bit PNG map images, and generates 32-bit PNG (or optionally TGA) location images
with alpha masks, which the above mentioned game versions support. The associated BIN files containing the
location image positions are also generated.


Basic functionality
-------------------

Start the tool and select a map directory. The directory must contain map page images (in true-color PNG format)
with the appropriate Dark-compliant names.

The tool will attempt to auto-detect Thief or System Shock 2 mode by looking if the files "PAGE001A.PNG" and
"PAGE001A-HI.PNG" are found, if they are it will assume SS2 mode, otherwise it will run in Thief mode. Forcing
a particular mode is possible through command-line options.

The main window consists of an image view, that displays the currently selected map page and defined locations,
and a list view that displays the map pages and locations for each page. A page or location is selected by
clicking on it in the list or right-clicking on it in the image view.

To create a new location just click on the image view with the left mouse button, that will create the first shape
point. Further left-clicks will create additional points. Define the desired shape and when you're satisfied
with it you can add it as a new location by pressing one of middle mouse button or RETURN. If you're not happy
with the in-progress shape you can right-click to remove the last added point or press ESC to abort the shape
entirely.

When a new location is created it will automatically be assigned an unused location index. To change the location
index you can select "Edit Location Index" from the "Edit" menu. Duplicate indexes are not allowed, so if you
enter an index that's already in use, it will ask if you want to swap the indexes.

The "View" menu has various options for the appearance. Locations can be drawn as outlines, filled with a color
or dimmed. Location index labels can be turned on off etc.

To save the current project select "Save Project" from the "Files" menu. There can only be one DarkMapGen project
per map directory, so it will not prompt for any filename or location. A file named "DarkMapGen.proj" will be
save to the current map directory. Likewise when starting the tool, it will automatically load the "DarkMapGen.proj"
file, should one be present in the current map directory.

To generate the location sub-images and BIN files, select "Generate Map Files" from the "Files" menu. All generated
files are output to the current map directory, overwriting any previously existing ones. The generation dialog
also has the option "Generate as TGA", which generates TGA images instead of PNG.


Editing
-------

To do any kind of editing of a location you first have to select one. The currently selected shape and points will
highlight in the image view.

To delete locations you have to select one and then pick "Delete Selected Location" from the "Edit" menu. It's
possible to delete all the locations for an entire page at once, by selecting the page in the list instead of
a specific location.

It's possible to modify the shapes of created locations, move, add or delete points, or change the position of
the index label. First make sure the location has been selected.

You can then hold down CTRL to move points. Just move the mouse over a point, while keeping CTRL pressed, until
the point highlights. The left-click and drag the point around.

In a similar fashion you can insert or delete points by holding down ALT instead. When you move the mouse over
an existing point until it highlights, the point can be deleted by left-clicking. Note that when a shape only has
3 points, all points will hilight, indicating that the entire shape will be deleted. If the mouse moves over an
edge, a new virtual point will highlight instead, which can be inserted by left-clicking.

Holding down both ALT and SHIFT can be used to delete an entire shape. Moving the mouse over a point will make
all points in the shape hilight, to indicate that left-clicking on it will delete the entire shape.

Entire locations can be moved using the "Move Selected Location" function from the "Edit" menu. If a location is
selected it will move that only, if a page is selected in the list then it will move all locations. The move
function will prompt for the delta X and Y movement to apply. Enter two space separated values.

It's possible to move the shape label(s) of the selected location around by holding down both CTRL and ALT.
The label that can be repositioned will highlight. If a location consists of mulitple shapes, you have to move
the mouse over the specific label you want to move and then drag it. The label position will default to the
center of a shape when the shape is created, but for more complex shapes that can result the label not being
placed optimally or even outside of the shape. In those situations it can be useful to move the label.

While dragging/moving a point, or when creating a new shape, a useful function is snapping. Snapping is enabled
by holding down SHIFT. The current mouse position will snap against any other shape point, if it's near one,
if not it will snap axially against the points of the current shape. This makes it easy to both create new shapes
that align with existing ones, and to create straight vertical or horizontal lines.


Complex shapes
--------------

Sometimes you may need shapes that have holes in them, or a location with several shapes that aren't connected.
To do this you select an existing location that you want to add new shapes or holes to. Then you create a new
shape as usual, but instead of pressing RETURN or the middle mouse button, you press INSERT or ALT + middle-click.

To create a hole in an existing shape you just make sure that the first point of the hole-shape is inside the
shape where you want the hole. If the first point on the other hand is outside of any shape in the selected
location, then it will add a new solid shape.


Thief specifics
---------------

The expected map image names are in the form "PAGExxx.PNG" (PAGE000.PNG, PAGE001.PNG, PAGE002.PNG etc.).
The generated location images will in the form of "PxxxRxxx.PNG", and "PxxxRA.BIN" for the rects file
(that contains the location image positions).


System Shock 2 specifics
------------------------

The expected map image names are "PAGE001A.PNG" and "PAGE001A-HI.PNG", both images must have the same dimensions.
"PAGE001A.PNG" is the normal map image that the game also uses. "PAGE001A-HI.PNG" is not used by the game but
needed by this tool, because SS2 has two sets of image location images. One set is for visited locations, and a
second set of the same images but with highlighted outlines, used for the location the player is currently in.
"PAGE001A-HI.PNG" should therefore be a copy of "PAGE001A.PNG" but with highlighted outlines.

The generated location images will in the form of "P001Rxxx.PNG" and ""P001Xxxx.PNG"", and "P001RA.BIN" and
"P001XA.BIN" for the rects files (that contain the location image positions).


Command-line options
--------------------

   -thief           : run in Thief mode without auto-detection
   -shock           : run in System Shock 2 mode without auto-detection
   -cwd             : use current working directory instead of prompting user
   -theme_gtk       : use the 'GTK+' UI theme (only available if another default theme is set)
   -theme_dark      : use the 'GTK+' UI theme with dark colors
   -theme_plastic   : use the 'Plastic' UI theme
   -theme_base      : use the basic UI theme
   -fontsize <n>    : custom font size for UI and labels, a value between 8 and 18 (default is 14)
   -scale <n>       : custom scale for the main window (0 = 1x, 1 = 1.25x, 2 = 1.5x, 3 = 1.75x, 4 = 2x)
   -winsize <W>x<H> : custom window size at start-up
   -viewmode <n>    : set initial display mode (1=outlines, 2=fill sel, 3=fill all, 4=dim all, 5=fade unsel)
   -zoom <n>        : set initial zoom level (1 to 6)
   -labels          : enable draw labels
   -cguides         : enable draw cursor guides
   -fillnew         : enable fill create-shape
   -thicklines      : enable thick lines
   -hidelines       : hide outlines of selected locations
   -margin <n>      : number of pixels to pad borders of generated location images (default is 0)
   -aa <n>          : anti-aliasing level used when creating the alpha mask for location images, a value
                      between 1 and 8 (default is 4)


Key summary
-----------

   [L--] = mouse button 1 (left)
   [--R] = mouse button 2 (right)
   [-M-] = mouse button 3 (middle)
   [-W-] = mouse wheel (up/down)


Shortcuts:

   <CTRL> + s       : save project
   <F7>             : generate map files
   <CTRL> + <F7>    : generate map files for selected location or page only (generating a single location still
                      needs to generate a full BIN file for that map page, so problems may arise if other location
                      changes were made on the page as well)
   <ALT> + x        : exit program

   <DEL>            : delete selected location
   <F9>             : edit location index
   m                : move selected location
   i                : display location information dialog

   <CTRL> + 1       : display mode - shape outlines only
   <CTRL> + 2       : display mode - fill shapes of selected location only
   <CTRL> + 3       : display mode - fill shapes for all locations
   <CTRL> + 4       : display mode - dim (and desaturate) shapes for all locations
   <CTRL> + 5       : display mode - make everything but the selected location look faded out
   <CTRL> + t       : toggle thick lines for shape outlines
   <CTRL> + l       : toggle drawing of location index labels
   <CTRL> + f       : toggle filled drawing of the in-progress shape
   <CTRL> + g       : toggle drawing of cursor guides
   <CTRL> + h       : toggle hiding of outlines for selected location (has no effect in outlines-only display mode)
   <NumPad_Minus>   : zoom out
   <NumPad_Plus>    : zoom in

   <PG_UP>          : change to previous map page
   <PG_DN>          : change to next map page

   <ALT> + [-W-]    : zoom in/out
   <CTRL> + [-W-]   : scroll view horizontally

Standard edit actions:

   [L--]           : create new in-progress shape point (the new point can be dragged by keeping the button pressed)
   [--R]           : select location under cursor, or clear location selection when clicking in an empty area

In-progress shape actions:

   [L--]           : add in-progress shape point

   <ESC>           : cancel the current in-progress shape

   <RET>           : add the in-progress shape as a new location
   [-M-]           : -- " --

   <INS>           : add the in-progress shape to the selected location (as an additional shape or hole)
   <ALT> + [-M-]   : -- " --

   [--R]           : delete last added point (cancels current in-progress shape when the last point is deleted)


Modifier keys:

   <SHIFT>          : snap cursor (during shape creation or when moving existing points)

   <CTRL>           : move existing point under cursor by left-click and drag (also works during shape creation)

   <ALT>            : left-click to delete point under cursor or insert new point in edge under cursor
   <ALT> + <SHIFT>  : same as above except it deletes the entire shape if cursor is over a point

   <CTRL> + <ALT>   : re-position location index label with left-click (hover over label if multi-label location)

   <SPACE>          : pan view (if scrollable) with left-click and drag


Disclaimer
----------

DarkMapGen is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Library General Public License for more details.
