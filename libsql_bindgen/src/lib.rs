#[no_mangle]
pub fn fib(n: i64) -> i64 {
    match n {
        0 | 1 => n,
        _ => fib(n - 1) + fib(n -2)
    }
}
