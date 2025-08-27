program RecordTest;
type
  Point = record
    x: integer;
    y: integer;
    test_val: array[0..5] of real;

  end;
var
  p: Point;
  z: integer;
  pz: array[0..10] of Point;

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
  p.test_val[0] := 25.0;
  p.test_val[1] := 50.0;
   z := p.x * p.y;
  printPoint(p);
  writeln('equals: ', z);
  writeln(p.test_val[0], ',', p.test_val[1]);
  pz[0].x := 5;
  pz[0].y := 10;
  writeln(pz[0].x,  '|', pz[0].y);
end.
