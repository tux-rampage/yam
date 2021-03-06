/*$VER:WholeStory.rexx   � Kai.Nikulainen                              email: kajun@sci.fi 1.6 (04-Sep-97)
Finds and joins selected messages into one textfile in a user selectable order.

** I belong to a couple of X-Files fanfic mailing list and long stories are 
** usually posted in parts.  This script will find and join all parts.
**
** NOTE: You have to have 'Delete on exit' option set on in YAM configuration.
**
** New in 1.6
**  -lets you change the order of the parts
**
** New in 1.5
**  -uses string matching do decide which messages to add
**
** Send all comments, bug reports, suggestions and X-mas cards to knikulai@utu.fi
*/

options results
call addlib('rexxsupport.library',0,-30)
call addlib('rexxreqtools.library',0,-30)

defdir='work:home'
 strbody='Enter string pattern for subjects'
 senbody='Should the sender be same than is this message?'
 ordbody='I found these parts in this order.  Select a part if it should be moved:'
 newbody='Before which part it should be moved?'
 renbody='Messages were appended to an existing file.' || '0a'x
 renbody=renbody||'Do you want to change the name?'
buts1='_Ok|_Use old name'
buts2='_Ignore sender|_Only this sender|_Exit'
buts3='_All in order|_Exit'
buts4='_Cancel'

  title='WholeStory 1.5 � kajun@sci.fi'
   tags='rt_pubscrname=YAMSCREEN'  /* Change here the name of the screen YAM runs */

mc=0 
used=0
NL='0a'x

address 'YAM'
'GetMailInfo From'
server=result
'GetMailInfo Subject'
subj=result
'GetMailInfo Active'
active=result
'GetFolderInfo Max'
n=result
pattern=rtgetstring('*'subj'*',strbody,title,,tags,button)
if button=0 then exit

se=rtezrequest(senbody,buts2,title,'rtez_defaultresponse=1' tags)
if se=0 then exit
ignore_sender=(se=1)

/* 
** First scan all messages in the folder.
** Search for messages with same sender and which match the pattern
*/

do m=0 to n-1
    'SetMail' m
    'GetMailInfo From'
    if result=server | ignore_sender then do
       'GetMailInfo Subject'
       if match(pattern,result) then do
                call AddMsg(result)
                part.mc=m
                end
    end /* if result=server then do */
end /* do m=1 to n */
'SetMail' active  /* Go back to the originally selected message */
/*
** Then sort the messages.  Hopefully part 10 comes after 9 and not after 1....
*/

call SortMessages


do until sel=mc+1
  call ListMsgs
  def=mc+1
  sel=rtezrequest(ordbody || msgs,buts || buts3,title,'rtez_defaultresponse='def tags)
  if sel=0 then exit
  if sel<def then do
        new=rtezrequest(newbody || msgs,buts || buts4,title,'rtez_defaultresponse=0' tags)
        if new~=sel & new~=sel+1 & new>0 then do
                n=1
                do o=1 to new-1
                        if o~=sel then do
                                su.n=subject.o
                                fi.n=filename.o
                                pa.n=part.o
                                n=n+1
                                end /* if */
                        end /* do o */
                su.n=subject.sel
                fi.n=filename.sel
                pa.n=part.sel
                n=n+1
                do o=new to mc
                        if o~=sel then do
                                su.n=subject.o
                                fi.n=filename.o
                                pa.n=part.o
                                n=n+1
                                end /* if */
                        end /* do o=new */
                do o=1 to mc
                        filename.o=fi.o
                        subject.o=su.o
                        part.o=pa.o
                        end /* do o=1 to mc */
                end /* if new~=sel */
        end
  end
/*
** Lets write the messages into one file
*/
storyname=compress(subject.1,',/:()*<>[]"0123456789')
if upper(left(storyname,3))='NEW' then storyname=substr(storyname,4)
if upper(left(storyname,6))='REPOST' then storyname=substr(storyname,7)
storyname=strip(left(storyname,25,''))
storyname=translate(storyname,'_',' ')
outfile=rtfilerequest(defdir,storyname,'Select story name' tags)
if outfile='' then exit

if exists(outfile) then do
        call open(out,outfile,'a')
        call writeln(out,'The following messages follow:')
        append=1
        end
else do
        call open(out,outfile,'w')
        call writeln(out,'This file contains the following messages:')
        append=0
        end

do i=1 to mc
        call writeln(out,subject.i)
        end /* do i=1 to mc */
call writeln(out,'')
address 'YAM' 
do i=1 to mc
        if ~open(inp,filename.i,'r') then do
                'Request "Can not read part*n'subject.i'" "Ok"'
                'SetMail' active  /* Go back to the originally selected message */ 
                exit
                end
        do until eof(inp) | r='' /* Skip headers */
                r=readln(inp)
                end
        do until eof(inp)
                r=readln(inp)
                call writeln(out,r)
                end
        call close(inp)
        'SetMail' part.i
        'MailDelete'
        end /* do i=1 to mc */
call close(out)
'Request "All parts are saved and marked as deleted!" "_Ok"'
if append then do
        newname=rtgetstring(outfile,renbody,title,buts1,tags,renbut)
        if renbut=1 then
                address command 'rename' outfile newname
        end
'SetMail' active  /* Go back to the originally selected message */
exit    /* It's the end of the script as we know it... */


ListMsgs:
  msgs=''
  buts=''
  do i=1 to mc
        msgs=msgs NL || right(i || ')',3) subject.i
        if i<10 then
                etu='_'
        else
                etu=''
        buts=buts || etu || i || '|'
        end
return

FindStart:
  found_it=0
  if used=0 then do
  /* 
  ** When called the first time, this branch checks which delimiters are used 
  ** Following calls use else branch
  */
     do until eof(1) | found_it
         rivi=readln(1)
         do d=1 to delimiters
             if pos(startline.d,rivi)=1 then do
                  found_it=1
                  used=d
                  end /* if */
              end /* do d=1 */
         end /*do until */
     if used=3 then call writeln(tmp,rivi)  /* Save uuencode first line */
     end /* ifused=0 */
  else do
     do until eof(1) | found_it
         rivi=readln(1)
         found_it=pos(startline.used,rivi)
         end /*do until */  
     end /* else do */
return found_it

AddMsg:
/*
** This changes the subject's (1/n) to (01/n) to allow sorting
*/
  parse arg s
  mc=mc+1
  bra=lastpos('(',s)
  if bra>0 then do
      slash=pos('/',s,bra)
      if slash=0 then do
          slash=lastpos('/',s)
          if slash>0 then bra=lastpos(' ',s,slash)
          end
      end
  else do
      slash=lastpos('/',s)
      if slash>0 then bra=lastpos(' ',s,slash)
      end
  if slash-bra=2 then s=left(s,bra)'00'substr(s,bra+1)
  if slash-bra=3 then s=left(s,bra)'0'substr(s,bra+1)
  subject.mc=s
  'GetMailInfo File'
  filename.mc=result
return

SortMessages:
/* 
** A simple algorithm is fastest with relatively few items.
** There should be no need for something fancy like quicksort :-) 
*/
  do i=2 to mc
     do j=1 to i-1
        if upper(subject.j)>upper(subject.i) then do/* let's swap stuff... */
                temp=subject.j
                subject.j=subject.i
                subject.i=temp
                temp=filename.j
                filename.j=filename.i
                filename.i=temp
                temp=part.j
                part.j=part.i
                part.i=temp
                end /* if */
     end /* do j */
  end /* do i */
return  /* Everything should be in order now... */

Match: procedure
parse arg pat,str
        res=0
        pat=upper(pat)
        str=upper(str)
        p1=pos('*',pat)
        if p1=0 then
                res=(pat=str)
        else do
                alku=left(pat,p1-1)     /* chars before first * */
                ale=length(alku)
                p2=lastpos('*',pat)
                if left(str,ale)~=alku then
                        res=0
                else 
                        if p1=length(pat) then 
                                res=1
                        else do
                                loppu=substr(pat,p1+1)
                                p2=pos('*',loppu)
                                if p2=0 then
                                        res=(right(str,length(loppu))=loppu)
                                else do
                                        seur=left(loppu,p2-1)
                                        i=ale
                                        do while pos(seur,str,i+1)>0
                                                i=pos(seur,str,i+1)
                                                res=(res | Match(loppu,substr(str,i)))
                                                end
                                        end
                                end /* else do */       
                        end
return res

