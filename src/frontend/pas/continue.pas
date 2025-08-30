program LoopTest;
var
i : integer;
begin

for i := 0 to 20 do
begin
	if i < 10 then continue;
	writeln('loop index: ', i);
end;

end.
