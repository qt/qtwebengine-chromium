---
title: Code Generation
draft: false
archetype: "chapter"
weight: 5
---

## ei-scanner

In the libei repo, we use the
[ei-scanner](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/proto/ei-scanner)
to generate bindings for C and Python (for tests) as well the [interface
documentation]({{% relref "interfaces/" %}}) in this documentation.

Note: these generated protocol bindings are **not** part of libei's API contract.

The `ei-scanner` is written in Python but is intended to be useful to anyone that needs generated
bindings. It parses the [`protocol.xml`](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/proto/protocol.xml)
file and passes its content as data to [Jinja](https://jinja.palletsprojects.com/).

See [here](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/src/ei-proto.c.tmpl)
for the template that generates the C source for libei and libeis.


{{% notice  style="primary" %}}
If you plan to use the `ei-scanner` to generate language bindings, please get
in contact with us through the [issue tracker](https://gitlab.freedesktop.org/libinput/libei/-/issues/).
{{% /notice %}}

### Usage
```
$ ei-scanner --help
usage: ei-scanner [-h] [--component {ei,eis,brei}] [--output OUTPUT]
                  [--jinja-extra-data JINJA_EXTRA_DATA]
                  [--jinja-extra-data-file JINJA_EXTRA_DATA_FILE]
                  protocol template

ei-scanner is a tool to parse the EI protocol description XML and
pass the data to a Jinja2 template. That template can then be
used to generate protocol bindings for the desired language.

typical usages:
     ei-scanner --component=ei protocol.xml my-template.tpl
     ei-scanner --component=eis --output=bindings.rs
        protocol.xml bindings.rs.tpl

Elements in the XML file are provided as variables with attributes
generally matching the XML file. For example, each interface has
requests, events and enums, and each of those has a name.

ei-scanner additionally provides the following values to the Jinja2 templates:
    - interface.incoming and interface.outgoing: maps to the
       requests/events of the interface, depending on the component.
    - argument.signature: a single-character signature type mapping
      from the protocol XML type:
        uint32 -> "u"
        int32 -> "i"
        float -> "f"
        fd -> "h"
        new_id -> "n"
        object -> "o"
        string -> "s"

ei-scanner adds the following Jinja2 filters for convenience:
    {{foo|c_type}} ... resolves to "struct foo *"
    {{foo|as_c_arg}} ... resolves to "struct foo *foo"
    {{foo_bar|camel}} ... resolves to "FooBar"

positional arguments:
  protocol              The protocol XML file
  template              The Jinja2 compatible template file

options:
  -h, --help            show this help message and exit
  --component {ei,eis,brei}
  --output OUTPUT       Output file to write to
  --jinja-extra-data JINJA_EXTRA_DATA
                        Extra data (in JSON format) to pass through
                        to the Jinja template as 'extra'
  --jinja-extra-data-file JINJA_EXTRA_DATA_FILE
                        Path to file with extra data to pass through
                        to the Jinja template as 'extra'

```
