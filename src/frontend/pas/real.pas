program RealValue;
var
x: real := 0.0;
i: integer := 0;
begin

for i := 0 to 10 do
begin
x := x + 0.1;
end;
writeln('value of x: ', x);
end.
