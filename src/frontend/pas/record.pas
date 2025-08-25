program RecordTest;
type
  Point = record
    x: integer;
    y: integer;
  end;
var
  p: Point;
  z: integer;
begin
p.x := 5;
p.y := 10;
z := p.x * p.y;
writeln('equals: ', z);
end.
