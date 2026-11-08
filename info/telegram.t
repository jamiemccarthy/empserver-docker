.TH Command TELEGRAM
.NA telegram "Send \*Qdiplomatic\*U communique to another country"
.LV Basic
.SY "telegram <CNUM/CNAME>..."
The telegram command allows communication
to take place between representatives.  Currently you
are given only 1024 characters per telegram.
.s1
The <CNUM/CNAME> argument is either the number or name of
the recipient nation. You may specify as many recipients on the
command line as you wish.
Telegrams sent to country #0 appear as prayers,
(favors are best obtained in this manner.)
.s1
While entering your telegram the system tells you how many characters
you have left.  You end the telegram by entering a line with a '.'
on a line by itself. A line containing
only '~q' will abort the telegram.
.s1
For example:
.EX tel 7 8
.NF
Enter telegram for multiple recipients
undo last line with ~u, print with ~p, abort with ~q, end with .
 1024 left:
 1023 left: Quillon Pennywhistle
 1002 left: Embassy of Marmalada
  981 left:
  980 left: Dear Ambassador Pennywhistle,
  950 left:
  949 left: Just a little note to express the friendly wishes that all of us
  884 left: in Curmudgeon feel toward you bastards in Marmalada.  As
  827 left: to the matter of the treaty you've proposed please remember that
  762 left: if we wished we could TRASH your capital.
  720 left:
  719 left:                         As always I remain,
  675 left:                         Your humble and obedient servant, etc, etc
  608 left:                         Bartholomew Fizzlewick
  561 left:                         Deputy Undersecretary for Paperclips
  500 left:                         the Sovereign State of Curmudgeon
  442 left: .
.FI
.s1
Note that others can reject your telegrams, using \*Qreject\*U.
.s1
.SA "flash, toggle, announce, read, reject, Communication, Diplomacy"
