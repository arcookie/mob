
-- The release date of the first version (ver. 0.1) is early april 2016. --

This project is to make a library which make all functionities of one program that based on SQLite DB to multi-users real-time collaboratable.  

This project is open source version of commercial module(that couldn't be opened), which made with same paradigm, not same source code, so it has no simularity in source code.

This project is based on SQLite and Alljoyn technology, and it is cross-platformable.

The first version(ver. 0.1) is developed on Visual studio 2013 IDE, the start-up source is shell.c in SQLite source code.

The result executable sample.exe is SQLite shell consol, the command is SQL sentence. that is, if a joiner join by running sample.exe and do a command of SQL sentence to consol, it change the connected DB tables, and the other joiner's SQLite DBs syncronized by mob.

SQL sentence and file(s) are syncronized, in case of file, the path most be included in SQL sentence as a formal URI format(start with 'files://').

There is no limit on collaboration joiners.


Generally to speak, integrity of collaboration programming is rely on three algorithm, missing recovery, collision resolve, and ordering.

In mob technology,

Missing recovery is accomplished by serial number per joiners, by requesting missed serial numbers's commands in received commands to the creater of that commands.

Collision resolve is, pile up of commands become command history, and if more then two commands created by different joiners share same pre-command in command history, then it is conflicted.

  To resolve this, Mob uses ordering by joiners name. In processing of resolution, mob uses undo of commands internally.

Ordering is, all of commands in command history (except the first one) have pre-command.

  If we could send command with joiner's ID and serial number, then by using this as a key, we could syncronize all command histories of joiners same.

By these we guarantee all commands created by all joiners applied with same order.

In Mob library, this three algorithm, missing recovery, collision resolve, and ordering are occured internally, and the user(programmer) of mob library doesn't know the details.


To open this project, one need Visual Studio 2013 (or over) IDE on MS-Windows.

you could download the source code in here.

After unzip, the structure of source path is

mob/include
   /bin
   /lib
   /src/mob
       /sample
	   /sqlite
   /windows

This project is needed alljoyn SDK(for windows, alljoyn-15.04.00b-win7x86vs2013-sdk).

Because of sizes of those libraries, I couldn't upload those to this repository.

You could download it on https://allseenalliance.org/framework/download at v15.04

In the zip file you could download, there are two kind of libraries, debug, release.

alljoyn-15.04.00b-win7x86vs2013-sdk/alljoyn-15.04.00b-win7x86vs2013-sdk-dbg
alljoyn-15.04.00b-win7x86vs2013-sdk/alljoyn-15.04.00b-win7x86vs2013-sdk-rel

To compile sample project you need to copy lib files to {root}/lib/debug, and {root}/lib/release for each.

Then open /windows/sample.sln solution. It include three projects, mob, sample, sqlite.

mob is mob library, sample is the sample (SQLite shell consol program), sqlite is most recently published sqlite source code.

After build the executable is created on  /bin/release (or /bin/debug)

The usage of this executable is for work owner

sample -s work_name(alpha-number)

for work joiner

sample -j work_name(alpha-number:created by work owner)

You could test it with normal SQL commands on it's consol.

The functions of mob library exported are six, it defined in /include/mob.h , used on shell.c at sample project.

You could understand and apply to the other program easily by referencing the code snap on shell.c source code.

