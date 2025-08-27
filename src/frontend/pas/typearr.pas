program RecordTest;
type
  Point = record
    x: integer;
    y: integer;
    test_val: array[0..10] of real;
  end;
var
  p: Point;
  z: integer;

procedure printPoint(pi: Point);
var
px: Point;
i: integer;
begin
  px.x := 10;
  px.y := 10;
  writeln(pi.x, ':', pi.y);
  writeln(px.x, ':', px.y);
  for i := 0 to 9 do
  begin
    px.test_val[i] := i + 0.1 * 100.5;
    writeln(px.test_val[i], 0.5);
  end;
end;


begin
  p.x := 5;
  p.y := 10;
  z := p.x * p.y;
  printPoint(p);
  writeln('equals: ', z);
  writeln(0.5);
end.
