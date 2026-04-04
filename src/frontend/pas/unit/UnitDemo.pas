{ UnitDemo - Demonstrates using multiple separately compiled Pascal units }
program UnitDemo;
uses io, MathLib, StringUtils;

var
  a, b, result: integer;
  i: integer;

begin
  PrintBanner('  MXVM Unit Demo  ', 20);
  writeln;

  { Basic arithmetic }
  PrintHeader('Arithmetic');
  a := 42;
  b := 17;
  PrintLabeledInt('a', a);
  PrintLabeledInt('b', b);
  PrintLabeledInt('Add(a, b)', Add(a, b));
  PrintLabeledInt('Subtract(a, b)', Subtract(a, b));
  PrintLabeledInt('Multiply(a, b)', Multiply(a, b));
  PrintLabeledInt('Divide(a, b)', Divide(a, b));
  PrintSeparator;
  writeln;

  { Power and factorial }
  PrintHeader('Power & Factorial');
  PrintLabeledInt('2^10', Power(2, 10));
  PrintLabeledInt('3^5', Power(3, 5));
  for i := 1 to 10 do
    PrintLabeledInt('Factorial', Factorial(i));
  PrintSeparator;
  writeln;

  { Min, Max, Abs }
  PrintHeader('Min / Max / Abs');
  PrintLabeledInt('Max(100, 200)', Max(100, 200));
  PrintLabeledInt('Min(100, 200)', Min(100, 200));
  PrintLabeledInt('Abs(-55)', Abs(-55));
  PrintLabeledInt('Abs(55)', Abs(55));
  PrintSeparator;
  writeln;

  { Division by zero safety }
  PrintHeader('Edge Cases');
  PrintLabeledInt('Divide(10, 0)', Divide(10, 0));
  PrintLabeledInt('Factorial(0)', Factorial(0));
  PrintLabeledInt('Power(5, 0)', Power(5, 0));
  PrintSeparator;
  writeln;

  PrintBanner('  Demo Complete  ', 20);
end.
