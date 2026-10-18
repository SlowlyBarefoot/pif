Integration Model {#integration_model}
=================

@tableofcontents

PIF sits between the platform code and the application code.

| Direction | Mechanism |
| --- | --- |
| Platform -> PIF | `sig*` functions (ticks, signals, inputs) |
| PIF -> Platform | `act*` callback pointers |
| Application -> PIF | public API calls |
| PIF -> Application | `evt*` callback pointers |

## Callback names

- `act*`: PIF asks the platform or the application to do something.
- `evt*`: PIF reports an event.

## Member visibility

C has no member visibility, so PIF marks it in the field names:

- `__name`: private. Only the owning module touches it.
- `_name`: read-only from outside. Read it, never write it.
- no prefix: public.
