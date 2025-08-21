program Fibonacci;
var
  n: integer;
  a, b, temp: integer;
  i: integer;
  code: integer;
begin
  a := 0;
  b := 1;
  i := 2;
  n := 92;
  write(a, ' ');
  if n > 1 then
    writeln(b, ' ');

  while i < n do
  begin
    temp := a + b;
    a := b;
    b := temp;
    writeln(b, ' ');
    i := i + 1;
  end;
  writeln(' done ');
end.
