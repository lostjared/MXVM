program RecordTest;
type
  Point = record
    x: integer;
    y: integer;
    value: string;
    test_val: real;
  end;
var
  p: Point;
  z: integer;

procedure printPoint(pi: Point);
var
px: Point;
begin
  px.x := 10;
  px.y := 10;
  writeln(pi.x, ':', pi.y);
  writeln(px.x, ':', px.y);
end;


begin
  p.x := 5;
  p.y := 10;
  p.test_val := 25.0;
  p.value := 'test 1 2 3';
  z := p.x * p.y;
  printPoint(p);
  writeln('equals: ', z);
  writeln(p.value);
  writeln(p.test_val);
end.
