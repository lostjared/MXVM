program RecordTest;
type
  Point = record
    x: integer;
    y: integer;
    value: string;
  end;
var
  p: Point;
  z: integer;

procedure printPoint(pi: Point);
begin
  write(pi.x, ':', pi.y);
end;

begin
p.x := 5;
p.y := 10;
p.value := 'test 1 2 3';
z := p.x * p.y;
printPoint(p);
writeln('equals: ', z);
writeln(p.value);
end.
