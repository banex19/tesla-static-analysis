# ThinTESLA: A low overhead temporal assertions framework
This is a completely new version of TESLA (see below), which addresses the performance concerns and the shortcomings of its predecessor. Please have a look at the dissertation describing the major changes and improvements here: https://github.com/banex19/thin-tesla/blob/master/ThinTESLA_Dissertation.pdf

# TESLA: Temporally Enhanced Security Logic Assertions

TESLA is a tool that allow programmers to add temporal assertions to
their security-critical code. Rather than simply asserting that
"a particular expression evaluates to true right now", programmers
can specify temporal properties such as "this access control check
occurs before that object is used".

A programmer's guide (including build instuctions) can be found at:
http://www.cl.cam.ac.uk/research/security/ctsrd/tesla

