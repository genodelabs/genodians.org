<p align="center">
<img alt="Genode-Logo" width="350" src="https://genodians.org/site_title.png">
</p>

[Genodians.org](https://genodians.org) is a community site for the Genode OS 
Framework and associated projects.

This repository contains the static content generator used for the
genodians.org website. The website aggregates content from several authors.
Each author is responsible for its own content, which is hosted independently
from this repository, under the control of the respective author.


## Participation

Genodians.org welcomes you as an author! Please consider the following
guidelines when participating:

**Stay on topic** Please keep in mind that genodians.org is a site about
  Genode. Your content should draw a connection to this topic.

**Civil articulation** Don't misuse genodians.org for political rumblings or
  unconstructive rants. Keep up a friendly and inviting tone.

**No ads** Genodians.org is an ad-free zone. Don't pollute it with ads served
  by any ad network.

**Keep file names intact** It goes without saying that you are free to alter
  your content after publishing, e.g., to correct typos or to incorporate
  critique. It is your content after all. But in order to keep links from
  others to your existing postings intact, please reconsider the renaming of
  any files.

Your content remains entirely your's. It will not be featured at genodians.org
without your consent. Vice versa, you are not entitled to be featured at
genodians.org. Since genodians.org is operated by Genode Labs, please keep in
mind that the aggregation of the content happens solely at the discretion of
Genode Labs.


## Content format

Each author maintains his/her own content, preferably in a public git
repository. You may take a look at [https://github.com/nfeske/genodian]
as an example.

All texts must be written using the GOSH syntax, which is documented at
[https://github.com/nfeske/gosh].

The first step is creating a new repository, which you may name "genodian".
Please add a _LICENSE_ file to your repository that permits for the
aggregation of your content, e.g., you may use the following snippet as a
template

```sh
 This work is licensed under the Creative Commons Attribution + ShareAlike
 License (CC-BY-SA). To view a copy of the license, visit
 http://creativecommons.org/licenses/by-sa/4.0/legalcode
```

The repository must contain an _author.txt_ file, which tells a few words
about you, e.g.,

- What is your background and relation to Genode?
- What are your technical interests?
- How to contact you?
- Links to your projects, if there are any

Furthermore, the repository must contain an _author.png_ file with an image
(128x128 pixels) of you or your avatar. Both the information in the
_author.txt_ file and the image will be displayed alongside your postings,
highlighting your authorship.

Each posting is a file named _YYYY-MM-DD-title-on-the-posting.txt_ where
_YYYY_ is the year, _MM_ the month, and _DD_ the day of month. As the title
will be used for posting's URL, keep it as concise and expressive as possible.


## Form of a posting

- The full title of a posting is given at the beginning of the text file in
  the form of a horizontally centered line of text. Note that the centering
  must be done with spaces, not tabs.

- The first paragraph is taken as a summary of the posting. It is displayed
  along the posting title on the front page and on the author's overview page.
  So it should ideally give a brief glimpse on the content - just enough to
  wet the appetite of the reader.

- Within a posting, please avoid using GOSH's document-structuring features
  (chapters, sections) except for named paragraphs. A named paragraph is a
  line of text underlined with '---' characters, which is useful to structure
  a larger posting into multiple sections.

- For code examples, please keep in mind that the site layout provides a
  horizontally limited content column, which fits approximately 70 monospace
  characters.

- If your posting contains quotes, consider using GOSH's markup for _italic_
  text by prefixing and suffixing each line of the quote with underscore
  characters. This way, quotes nicely stand out from your own text.

Please test your posting offline for visual glitches (see below for how that
works) before publishing it.


## Adding your content to genodians.org

Please open an [issue](https://github.com/genodelabs/genodians.org) and
provide the following information:

- A link to your content (i.e., a GitHub project). The content should already
  contain at least one posting to give an impression of you as author.

- Your real name. It will be displayed aside your articles. Pseudonyms are not
  accepted.


## Test driving the static site generator

For each author, there exists a subdirectory under _authors/_. It contains the
following files:

:_name_: Real-world name

:_zip_url_: URL to the author's content as zip file

:_flair_: Hint about the role within the community (to be assigned by the
  operator of genodians.org)

The genodians.org website fetches the content from all authors in periodic
intervals and makes it available as static HTML pages.

You can generate a version of the website with your content via the following
steps:

 Clone the genodians.org repository

```sh
   $ git clone https://github.com/genodelabs/genodians.org.git

   $ cd genodians.org
```


 Fetch the required sub module(s)

```sh
   $ git submodule update --init --recursive
```
 Make sure that you have Tcl installed (needed by the GOSH tool).

 Create a directory at 

```sh
authors/<your-username/
```
with the files mentioned above.

Add your content to the 

```sh
content/<your-username>/ 
```
subdirectory where

```sh
<your-username>
``` 
corresponds to your identity, e.g., your GitHub username.

  As a convenient practice, you may clone your content repository directly to
  this directory. For example, to incorporate nfeske's content, you may clone
  his genodian repository as follows

```sh
  $ git clone https://github.com/nfeske/genodian content/nfeske
```
 Generate the static HTML pages:

```sh
   $ make
```

 Once finished, you may view the generated pages via your web browser, e.g.,

```sh
   $ firefox ./html/index
```
