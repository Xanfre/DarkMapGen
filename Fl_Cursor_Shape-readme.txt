
Fl_Cursor_Shape

Fl_Cursor_Shape allows arbitrary cursor shapes for FLTK
running on Windows95/98/NT/2000 and XWindows (Linux tested).

Compilation should be obvious. Fl_Cursor_Shape does not
require any changes to the FLTK source tree.

Use "testCursorShape" to create new mouse cursors:
The left mouse button draws a white point, the middle
one draws a black point. The right mouse button creates
transparent points. Holding the shift key moves the hotspot.

Drag the mouse over the 'test' widget to see the pointer 
in its real size.

"Save" writes the cursor shape in a format that can
be included in C and C++ programs.


void fl_cursor(Fl_Cursor_Shape *c);
*** set a cursor shape for the topmost application window
 
void Fl_Cursor_Shape::shape(..)
*** set the shape using an 'and' and a 'xor' plane. 

void Fl_Cursor_Shape::color()
*** change the cursor colors (X-Windows only!)


Have fun ;-) !


Fl_Cursor_Shape is licensed under LGPL. Please send comments and requests to:

matthias@mediaone.net



