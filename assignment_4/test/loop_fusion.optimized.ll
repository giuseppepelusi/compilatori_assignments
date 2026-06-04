; ModuleID = 'test/loop_fusion.ll'
source_filename = "test/loop_fusion.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_fusible_constant(ptr noundef %0, ptr noundef %1) #0 {
  br label %3

3:                                                ; preds = %15, %2
  %.012 = phi i32 [ 0, %2 ], [ %16, %15 ]
  %4 = mul nsw i32 %.012, 2
  %5 = sext i32 %.012 to i64
  %6 = getelementptr inbounds i32, ptr %0, i64 %5
  store i32 %4, ptr %6, align 4
  %7 = mul nsw i32 %.012, 3
  %8 = sext i32 %.012 to i64
  %9 = getelementptr inbounds i32, ptr %1, i64 %8
  store i32 %7, ptr %9, align 4
  br label %10

10:                                               ; preds = %3
  %11 = add nsw i32 %.012, 1
  %12 = icmp slt i32 %11, 100
  br i1 %12, label %15, label %13, !llvm.loop !6

13:                                               ; preds = %10
  br label %14

14:                                               ; preds = %13
  br label %15

15:                                               ; preds = %10, %14
  %16 = add nsw i32 %.012, 1
  %17 = icmp slt i32 %16, 100
  br i1 %17, label %3, label %18, !llvm.loop !8

18:                                               ; preds = %15
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_different_trip_count(ptr noundef %0, ptr noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  %5 = icmp slt i32 0, %2
  br i1 %5, label %.lr.ph, label %12

.lr.ph:                                           ; preds = %4
  br label %6

6:                                                ; preds = %.lr.ph, %9
  %.012 = phi i32 [ 0, %.lr.ph ], [ %10, %9 ]
  %7 = sext i32 %.012 to i64
  %8 = getelementptr inbounds i32, ptr %0, i64 %7
  store i32 %.012, ptr %8, align 4
  br label %9

9:                                                ; preds = %6
  %10 = add nsw i32 %.012, 1
  %11 = icmp slt i32 %10, %2
  br i1 %11, label %6, label %._crit_edge, !llvm.loop !9

._crit_edge:                                      ; preds = %9
  br label %12

12:                                               ; preds = %._crit_edge, %4
  %13 = icmp slt i32 0, %3
  br i1 %13, label %.lr.ph5, label %21

.lr.ph5:                                          ; preds = %12
  br label %14

14:                                               ; preds = %.lr.ph5, %18
  %.03 = phi i32 [ 0, %.lr.ph5 ], [ %19, %18 ]
  %15 = mul nsw i32 %.03, 2
  %16 = sext i32 %.03 to i64
  %17 = getelementptr inbounds i32, ptr %1, i64 %16
  store i32 %15, ptr %17, align 4
  br label %18

18:                                               ; preds = %14
  %19 = add nsw i32 %.03, 1
  %20 = icmp slt i32 %19, %3
  br i1 %20, label %14, label %._crit_edge6, !llvm.loop !10

._crit_edge6:                                     ; preds = %18
  br label %21

21:                                               ; preds = %._crit_edge6, %12
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_fusible_raw_same_iter(ptr noundef %0, ptr noundef %1) #0 {
  br label %3

3:                                                ; preds = %18, %2
  %.012 = phi i32 [ 0, %2 ], [ %19, %18 ]
  %4 = mul nsw i32 %.012, 2
  %5 = sext i32 %.012 to i64
  %6 = getelementptr inbounds i32, ptr %0, i64 %5
  store i32 %4, ptr %6, align 4
  %7 = sext i32 %.012 to i64
  %8 = getelementptr inbounds i32, ptr %0, i64 %7
  %9 = load i32, ptr %8, align 4
  %10 = add nsw i32 %9, 1
  %11 = sext i32 %.012 to i64
  %12 = getelementptr inbounds i32, ptr %1, i64 %11
  store i32 %10, ptr %12, align 4
  br label %13

13:                                               ; preds = %3
  %14 = add nsw i32 %.012, 1
  %15 = icmp slt i32 %14, 100
  br i1 %15, label %18, label %16, !llvm.loop !11

16:                                               ; preds = %13
  br label %17

17:                                               ; preds = %16
  br label %18

18:                                               ; preds = %13, %17
  %19 = add nsw i32 %.012, 1
  %20 = icmp slt i32 %19, 100
  br i1 %20, label %3, label %21, !llvm.loop !12

21:                                               ; preds = %18
  ret void
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
!12 = distinct !{!12, !7}
