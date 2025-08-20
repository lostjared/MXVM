program read_line; 
var
x: integer := 0;
begin
	write('how many times to loop? ');
	readln(x);
	for i := 0 to x do
	begin
		writeln('looping: ', i);
	end;
end.
