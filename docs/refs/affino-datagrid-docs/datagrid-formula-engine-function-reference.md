# DataGrid Formula Engine Function Reference

This reference documents the current built-in formula surface in `@affino/datagrid-formula-engine`.

## General rules

- Function names are case-insensitive and normalize to uppercase internally.
- Most aggregate helpers flatten array arguments produced by `ARRAY(...)`, `RANGE(...)`, row-aware windows, or lookup helpers.
- Numeric helpers use the engine's normal coercion rules, so text like `'10'` can participate as `10`, `TRUE` behaves like `1`, and blank values usually behave like `0`.
- Collection and lookup helpers use **1-based indexing**.

## Criteria syntax used by conditional functions

The conditional helpers `COUNTIF`, `COUNTIFS`, `SUMIF`, `SUMIFS`, `AVERAGEIF`, and `COLLECT` support scalar criteria and operator-prefixed text criteria.

Supported forms:

- exact match: `'Open'`
- equality with explicit operator: `'=Open'`
- inequality: `'<>Closed'`, `'!=Closed'`
- numeric/date comparisons: `'>=10'`, `'<2026-03-01'`

Examples:

- `COUNTIF(statuses, 'Open')`
- `SUMIF(amounts, '>100')`
- `COLLECT(prices, region, 'EU', quantity, '>=10')`

## Numeric and statistical functions

| Function | Description | Example |
| --- | --- | --- |
| `ABS(x)` | Returns the absolute numeric value. | `ABS(-42)` |
| `AVG(...)` | Returns the arithmetic mean of all flattened arguments. | `AVG(price, tax, discount)` |
| `AVERAGE(...)` | Alias of `AVG(...)`. | `AVERAGE(ARRAY(10, 20, 30))` |
| `AVGW(values, weights)` | Returns the weighted average of two arrays or scalars, matched by position. | `AVGW(ARRAY(80, 90), ARRAY(1, 2))` |
| `CEIL(value, significance?)` | Rounds up to the next multiple of the significance. | `CEIL(12.2, 0.5)` |
| `CEILING(value, significance?)` | Alias of `CEIL(...)`. | `CEILING(12.2, 5)` |
| `CHAR(code)` | Converts a character code into a single-character string. | `CHAR(65)` |
| `COUNT(...)` | Counts present values across flattened arguments. | `COUNT(price, tax, ARRAY(bonus, fee))` |
| `COUNTM(...)` | Alias of `COUNT(...)`. | `COUNTM(ARRAY(code, status, owner))` |
| `DECTOHEX(value)` | Converts a decimal integer to uppercase hexadecimal text. | `DECTOHEX(255)` |
| `FLOOR(value, significance?)` | Rounds down to the previous multiple of the significance. | `FLOOR(12.9, 0.5)` |
| `HEXTODEC(text)` | Converts hexadecimal text to a decimal number. | `HEXTODEC('FF')` |
| `INT(value)` | Truncates the fractional part of a number. | `INT(12.9)` |
| `LARGE(values, rank)` | Returns the `rank`-th largest value from an array. | `LARGE(ARRAY(4, 9, 2, 7), 2)` |
| `MAX(...)` | Returns the largest numeric value from flattened arguments. | `MAX(price, tax, bonus)` |
| `MEDIAN(...)` | Returns the median of flattened numeric arguments. | `MEDIAN(ARRAY(10, 20, 30, 40))` |
| `MIN(...)` | Returns the smallest numeric value from flattened arguments. | `MIN(price, tax, bonus)` |
| `MOD(left, right)` | Returns the remainder after division. | `MOD(17, 5)` |
| `MROUND(value, multiple)` | Rounds to the nearest multiple. | `MROUND(17, 5)` |
| `NPV(rate, cashFlow1, cashFlow2, ...)` | Returns the discounted net present value of future cash flows. | `NPV(0.1, -1000, 400, 400, 400)` |
| `PERCENTILE(values, percentile)` | Returns the interpolated percentile for values with percentile in the `0..1` range. | `PERCENTILE(ARRAY(10, 20, 30, 40), 0.75)` |
| `POW(base, exponent)` | Raises a base to a power. | `POW(2, 8)` |
| `RANKAVG(value, values, order?)` | Returns the average rank of matching values; descending order is the default. | `RANKAVG(90, ARRAY(100, 90, 90, 80))` |
| `RANKEQ(value, values, order?)` | Returns the first matching rank; descending order is the default. | `RANKEQ(90, ARRAY(100, 90, 90, 80))` |
| `ROUND(value, digits?)` | Rounds to the requested number of decimal digits. | `ROUND(12.3456, 2)` |
| `ROUNDDOWN(value, digits?)` | Truncates toward zero at the requested precision. | `ROUNDDOWN(12.3456, 2)` |
| `ROUNDUP(value, digits?)` | Rounds away from zero at the requested precision. | `ROUNDUP(12.341, 2)` |
| `SMALL(values, rank)` | Returns the `rank`-th smallest value from an array. | `SMALL(ARRAY(4, 9, 2, 7), 2)` |
| `STDEVA(...)` | Returns sample standard deviation across flattened numeric values. | `STDEVA(ARRAY(1, 2, 3))` |
| `STDEVP(...)` | Returns population standard deviation across flattened numeric values. | `STDEVP(ARRAY(1, 2, 3))` |
| `STDEVPA(...)` | Alias of population standard deviation behavior. | `STDEVPA(ARRAY(1, 2, 3))` |
| `STDEVS(...)` | Alias of sample standard deviation behavior. | `STDEVS(ARRAY(1, 2, 3))` |
| `SUM(...)` | Returns the sum of flattened numeric arguments. | `SUM(price, tax, ARRAY(shipping, fee))` |
| `UNICHAR(codePoint)` | Converts a Unicode code point into text. | `UNICHAR(128512)` |

## Logical and predicate functions

| Function | Description | Example |
| --- | --- | --- |
| `AND(...)` | Returns `TRUE` when every argument is truthy after boolean coercion. | `AND(qty > 0, status == 'Open')` |
| `COALESCE(v1, v2, ...)` | Returns the first non-blank value, preserving `0`, `FALSE`, and empty text. | `COALESCE(discount, promoDiscount, 0)` |
| `CONTAINS(needle, valueOrArray)` | Returns `TRUE` when the text occurs inside any scalar or array entry, case-insensitively. | `CONTAINS('sku', code)` |
| `COUNTIF(range, criterion)` | Counts entries that match the criterion. | `COUNTIF(statuses, 'Open')` |
| `HAS(array, value)` | Returns `TRUE` when the value exists in the array. | `HAS(ARRAY('A', 'B', 'C'), code)` |
| `IF(condition, whenTrue, whenFalse)` | Returns one of two values based on a condition. | `IF(qty > 0, price * qty, 0)` |
| `IFERROR(value, fallback)` | Replaces an existing typed formula error value with a fallback. | `IFERROR(result, 0)` |
| `IFS(cond1, value1, cond2, value2, ...)` | Returns the value for the first truthy condition. | `IFS(score >= 90, 'A', score >= 80, 'B', TRUE, 'C')` |
| `IN(value, option1, option2, ...)` | Returns `1` when the value matches any option, otherwise `0`. | `IN(status, 'Open', 'Pending', 'Review')` |
| `ISBLANK(value)` | Returns `TRUE` when the value is blank (`null` or missing). | `ISBLANK(owner)` |
| `ISBOOLEAN(value)` | Returns `TRUE` when the value is a boolean. | `ISBOOLEAN(isActive)` |
| `ISDATE(value)` | Returns `TRUE` when the value is a valid `Date`. | `ISDATE(createdAt)` |
| `ISEVEN(value)` | Returns `TRUE` when the value is an even integer. | `ISEVEN(orderIndex)` |
| `ISERROR(value)` | Returns `TRUE` when the value is a typed formula error. | `ISERROR(result)` |
| `ISNUMBER(value)` | Returns `TRUE` when the value is numeric. | `ISNUMBER(price)` |
| `ISODD(value)` | Returns `TRUE` when the value is an odd integer. | `ISODD(orderIndex)` |
| `ISTEXT(value)` | Returns `TRUE` when the value is text. | `ISTEXT(code)` |
| `NOT(value)` | Returns the inverse boolean value. | `NOT(isArchived)` |
| `OR(...)` | Returns `TRUE` when any argument is truthy after boolean coercion. | `OR(priority == 'High', overdue > 0)` |

## Text functions

| Function | Description | Example |
| --- | --- | --- |
| `CONCAT(...)` | Concatenates flattened arguments into one string. | `CONCAT(firstName, ' ', lastName)` |
| `FIND(searchFor, text, startPosition?)` | Returns the 1-based position of a substring, or `0` when not found. | `FIND('-', orderCode)` |
| `JOIN(values, delimiter?)` | Joins an array or scalar list into one string. | `JOIN(ARRAY('A', 'B', 'C'), '-')` |
| `LEFT(text, count?)` | Returns the leftmost characters from a string. | `LEFT(code, 3)` |
| `LEN(value)` | Returns string length for scalars or item count for arrays. | `LEN(description)` |
| `LOWER(text)` | Converts text to lowercase. | `LOWER(email)` |
| `MID(text, start, count)` | Returns a substring using 1-based indexing. | `MID(code, 2, 4)` |
| `REPLACE(text, start, count, nextText)` | Replaces a substring at a 1-based position. | `REPLACE('ABCD', 2, 2, '##')` |
| `RIGHT(text, count?)` | Returns the rightmost characters from a string. | `RIGHT(code, 4)` |
| `SUBSTITUTE(text, oldText, newText, occurrence?)` | Replaces all occurrences, or only the selected occurrence when `occurrence` is provided. | `SUBSTITUTE('A-B-B', 'B', 'X', 2)` |
| `TRIM(text)` | Removes leading and trailing whitespace. | `TRIM(customerName)` |
| `UPPER(text)` | Converts text to uppercase. | `UPPER(code)` |
| `VALUE(value)` | Coerces a scalar into a numeric value. | `VALUE('42.5')` |

## Date and time functions

| Function | Description | Example |
| --- | --- | --- |
| `DATE(year, month, day)` | Creates a UTC date from year, month, and day. | `DATE(2026, 3, 9)` |
| `DATEONLY(value)` | Strips the time component from a date-like value. | `DATEONLY(createdAt)` |
| `DAY(date)` | Returns the day of month in UTC. | `DAY(DATE(2026, 3, 9))` |
| `MONTH(date)` | Returns the month number in UTC. | `MONTH(DATE(2026, 3, 9))` |
| `NETDAYS(start, end)` | Returns the calendar day difference between two dates, excluding the end-point inclusion adjustment. | `NETDAYS(DATE(2026, 3, 1), DATE(2026, 3, 10))` |
| `NETWORKDAY(start, end, holidays...)` | Counts working days inclusively, excluding weekends and optional holidays. | `NETWORKDAY(startDate, endDate, DATE(2026, 3, 8))` |
| `NETWORKDAYS(start, end, holidays...)` | Alias of `NETWORKDAY(...)`. | `NETWORKDAYS(startDate, endDate)` |
| `TIME(value)` | Parses a time-like or date-like scalar into a `Date`. | `TIME('14:30:00')` |
| `TODAY(offset?)` | Returns today's UTC date, optionally shifted by a number of days. | `TODAY(7)` |
| `WEEKDAY(date)` | Returns weekday number with Sunday = `1` and Saturday = `7`. | `WEEKDAY(DATE(2026, 3, 9))` |
| `WEEKNUMBER(date)` | Returns the simple week-of-year number where January 1 is always in week 1. | `WEEKNUMBER(DATE(2026, 3, 9))` |
| `WORKDAY(start, days, holidays...)` | Moves forward or backward by working days, skipping weekends and optional holidays. | `WORKDAY(DATE(2026, 3, 9), 5)` |
| `YEAR(date)` | Returns the UTC year number. | `YEAR(DATE(2026, 3, 9))` |
| `YEARDAY(date)` | Returns the 1-based day number inside the year. | `YEARDAY(DATE(2026, 3, 9))` |

## Arrays, lookup, and conditional aggregate functions

| Function | Description | Example |
| --- | --- | --- |
| `ARRAY(...)` | Builds a frozen 1-dimensional array from flattened arguments. | `ARRAY(price, tax, shipping)` |
| `AVERAGEIF(criteriaRange, criterion, averageRange?)` | Averages values whose paired criteria entries match. | `AVERAGEIF(statuses, 'Open', amounts)` |
| `CHOOSE(index, option1, option2, ...)` | Returns the 1-based option selected by index. | `CHOOSE(2, 'Low', 'Medium', 'High')` |
| `COLLECT(values, criteriaRange1, criterion1, ...)` | Returns an array of values whose paired criteria all match. | `COLLECT(amounts, status, 'Open', region, 'EU')` |
| `COUNTIFS(range1, criterion1, range2, criterion2, ...)` | Counts rows where every criterion pair matches the same position. | `COUNTIFS(status, 'Open', region, 'EU')` |
| `DISTINCT(values)` | Returns the first occurrence of each distinct value, preserving order. | `DISTINCT(ARRAY('A', 'B', 'A', 'C'))` |
| `INDEX(values, index, fallback?)` | Returns the 1-based element at `index`, or the fallback when out of range. | `INDEX(ARRAY('A', 'B', 'C'), 2, 'n/a')` |
| `MATCH(needle, values, matchMode?)` | Returns the 1-based exact match position. Only `matchMode = 0` is currently supported. | `MATCH(code, ARRAY('A', 'B', 'C'))` |
| `RANGE(...)` | Alias-style array builder used for aggregate pipelines and row-aware windows. | `RANGE(price, tax, shipping)` |
| `SUMIF(criteriaRange, criterion, sumRange?)` | Sums values whose paired criteria entries match. | `SUMIF(statuses, 'Open', amounts)` |
| `SUMIFS(sumRange, criteriaRange1, criterion1, ...)` | Sums values only when every criterion pair matches the same position. | `SUMIFS(amounts, status, 'Open', region, 'EU')` |
| `TABLE(name, fieldPath?)` | Reads a registered external table into a 1D formula array so dashboard formulas can aggregate across datasets. | `SUMIF(TABLE('orders', 'status'), 'Open', TABLE('orders', 'amount'))` |
| `VLOOKUP(needle, values, columnNumber, exact?)` | Compatibility subset of `VLOOKUP`: exact-only lookup over a 1-dimensional array, with `columnNumber = 1` only. | `VLOOKUP('A', ARRAY('A', 'B', 'C'), 1, 0)` |
| `XLOOKUP(needle, lookupValues, returnValues, notFound?, matchMode?)` | Returns the paired value from `returnValues` for an exact match in `lookupValues`. | `XLOOKUP(code, skuList, priceList, 0)` |

## Notes on current compatibility boundaries

- `TABLE(...)` is the new cross-table primitive. In `ClientRowModel`, register datasets through `setFormulaTable(name, rows)` and invalidate them through `removeFormulaTable(name)`.
- `TABLE(...)` is stable for single dashboard metrics and formula-engine level aggregation. Multi-metric same-level dashboard rows in `ClientRowModel` still need scheduler hardening before they can be treated as fully spreadsheet-class.
- `IFERROR(...)` only replaces values that are already materialized as typed formula errors. It is not a full lazy spreadsheet-style error trap yet.
- `MATCH(...)` and `XLOOKUP(...)` currently support exact matching only.
- `VLOOKUP(...)` is intentionally narrow today and should be treated as a compatibility bridge rather than a full spreadsheet table lookup.
- Hierarchy and project-management helpers such as parent/child/successor-aware formulas still require runtime context plumbing and are not part of the current built-in set.
