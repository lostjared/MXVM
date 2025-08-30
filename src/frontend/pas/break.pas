program LoopTest;
var
i : integer;
begin

for i := 0 to 20 do
begin
	if i > 10 then break;
	writeln(' index: ', i);
end;

end.
