; ModuleID = 'assign.c'
source_filename = "assign.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; 获取指针，然后load/store

; int main()
; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @main() #0 {
  ; default
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ; int a[10];
  %2 = alloca [10 x i32], align 4
  ; a[0] = 10;
  %3 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 0
  store i32 10, i32* %3, align 4
  ; a[1] = a[0] * 2;
  %4 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 0
  %5 = load i32, i32* %4, align 4
  %6 = mul nsw i32 %5, 2
  %7 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 1
  store i32 %6, i32* %7, align 4
  ; return a[1];
  %8 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 1
  %9 = load i32, i32* %8, align 4
  ret i32 %9
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0-4ubuntu1 "}
