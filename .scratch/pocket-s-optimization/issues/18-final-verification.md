# 18: Verify the result and write it down

Status: open
Type: task
Label: ready-for-agent
Blocked by: 16, 17

## Steps

1. Build a release APK from `master`. Install it. Move `config.yml` away
   as in ticket 16, so the preset applies. Put it back at the end.
2. Run the ticket 06 benchmark on all 4 titles with the protocol, and the
   20 minute run. Compare with the ticket 06 baseline.
3. Check each success criterion in `../spec.md`. Mark each as met or not
   met, with the numbers.
4. Run `container/vita3k.sh test`.
5. Add an "Ayaneo Pocket S" section to `README.md` for users: the
   recommended driver, what the preset sets, and how to reset it.
6. Add the device facts and the main results to `CLAUDE.md`, in at most 10
   lines.

## Output

Under `## Answer`: the baseline and final table, the criteria list, and the
list of commits made for this plan.

## Answer
