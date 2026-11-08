Hi Bob,

Thanks for the quick review! A few notes on the **release plan**:

1. Branch is cut on *Friday*
2. Tests must be green, see [the dashboard](https://ci.example.com/proj)
3. Ship on Monday

| Item  | Owner | Status        |
|-------|-------|---------------|
| Docs  | Alice | ~~open~~ done |
| Build | Bob   | open          |

Run `make release` or:

```sh
./release.sh --dry-run
```

Best regards,
Alice

On Mon, 2 Nov 2026 at 09:15, Bob Jones <bob@example.com> wrote:

> Looks good overall. Can we ship next week?
>
> /Bob
