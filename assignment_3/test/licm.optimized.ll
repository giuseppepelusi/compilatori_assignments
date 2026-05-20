; ModuleID = 'test/licm.ll'
source_filename = "test/licm.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@a = dso_local global i32 10, align 4
@b = dso_local global i32 20, align 4

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_basic(i32 noundef %0) #0 {
  %2 = load i32, ptr @a, align 4
  %3 = load i32, ptr @b, align 4
  %4 = add nsw i32 %2, %3
  %5 = load i32, ptr @a, align 4
  %6 = load i32, ptr @b, align 4
  br label %7

7:                                                ; preds = %13, %1
  %.01 = phi i32 [ 0, %1 ], [ %12, %13 ]
  %.0 = phi i32 [ 0, %1 ], [ %14, %13 ]
  %8 = icmp slt i32 %.0, %0
  br i1 %8, label %9, label %15

9:                                                ; preds = %7
  %10 = mul nsw i32 %5, %6
  %11 = add nsw i32 %10, %.0
  %12 = add nsw i32 %.01, %11
  br label %13

13:                                               ; preds = %9
  %14 = add nsw i32 %.0, 1
  br label %7, !llvm.loop !6

15:                                               ; preds = %7
  ret i32 %.01
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_chain(i32 noundef %0) #0 {
  %2 = load i32, ptr @a, align 4
  %3 = load i32, ptr @b, align 4
  br label %4

4:                                                ; preds = %11, %1
  %.01 = phi i32 [ 0, %1 ], [ %10, %11 ]
  %.0 = phi i32 [ 0, %1 ], [ %12, %11 ]
  %5 = icmp slt i32 %.0, %0
  br i1 %5, label %6, label %13

6:                                                ; preds = %4
  %7 = add nsw i32 %2, %3
  %8 = mul nsw i32 %7, 2
  %9 = add nsw i32 %8, %.0
  %10 = add nsw i32 %.01, %9
  br label %11

11:                                               ; preds = %6
  %12 = add nsw i32 %.0, 1
  br label %4, !llvm.loop !8

13:                                               ; preds = %4
  ret i32 %.01
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_not_invariant(i32 noundef %0) #0 {
  br label %2

2:                                                ; preds = %7, %1
  %.01 = phi i32 [ 0, %1 ], [ %6, %7 ]
  %.0 = phi i32 [ 0, %1 ], [ %8, %7 ]
  %3 = icmp slt i32 %.0, %0
  br i1 %3, label %4, label %9

4:                                                ; preds = %2
  %5 = mul nsw i32 %.0, 3
  %6 = add nsw i32 %.01, %5
  br label %7

7:                                                ; preds = %4
  %8 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !9

9:                                                ; preds = %2
  ret i32 %.01
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_nested(i32 noundef %0) #0 {
  %2 = load i32, ptr @a, align 4
  %3 = load i32, ptr @b, align 4
  br label %4

4:                                                ; preds = %17, %1
  %.02 = phi i32 [ 0, %1 ], [ %18, %17 ]
  %.01 = phi i32 [ 0, %1 ], [ %.1, %17 ]
  %5 = icmp slt i32 %.02, %0
  br i1 %5, label %6, label %19

6:                                                ; preds = %4
  %7 = mul nsw i32 %2, %3
  br label %8

8:                                                ; preds = %14, %6
  %.1 = phi i32 [ %.01, %6 ], [ %13, %14 ]
  %.0 = phi i32 [ 0, %6 ], [ %15, %14 ]
  %9 = icmp slt i32 %.0, %0
  br i1 %9, label %10, label %16

10:                                               ; preds = %8
  %11 = add nsw i32 %7, %.02
  %12 = add nsw i32 %11, %.0
  %13 = add nsw i32 %.1, %12
  br label %14

14:                                               ; preds = %10
  %15 = add nsw i32 %.0, 1
  br label %8, !llvm.loop !10

16:                                               ; preds = %8
  br label %17

17:                                               ; preds = %16
  %18 = add nsw i32 %.02, 1
  br label %4, !llvm.loop !11

19:                                               ; preds = %4
  ret i32 %.01
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7 (/home/runner/work/llvm-project/llvm-project/clang cd708029e0b2869e80abe31ddb175f7c35361f90)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
!9 = distinct !{!9, !7}
!10 = distinct !{!10, !7}
!11 = distinct !{!11, !7}
